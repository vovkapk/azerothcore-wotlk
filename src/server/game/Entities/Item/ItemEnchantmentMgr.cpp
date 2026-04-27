/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ItemEnchantmentMgr.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "Timer.h"
#include "Util.h"
#include <cmath>
#include <functional>
#include <vector>

struct EnchStoreItem
{
    uint32  ench;
    float   chance;

    EnchStoreItem()
        : ench(0), chance(0) {}

    EnchStoreItem(uint32 _ench, float _chance)
        : ench(_ench), chance(_chance) {}
};

typedef std::vector<EnchStoreItem> EnchStoreList;
typedef std::unordered_map<uint32, EnchStoreList> EnchantmentStore;

static EnchantmentStore RandomItemEnch;

void LoadRandomEnchantmentsTable()
{
    uint32 oldMSTime = getMSTime();

    RandomItemEnch.clear();                                 // for reload case

    //                                                 0      1      2
    QueryResult result = WorldDatabase.Query("SELECT entry, ench, chance FROM item_enchantment_template");

    if (result)
    {
        uint32 count = 0;

        do
        {
            Field* fields = result->Fetch();

            uint32 entry = fields[0].Get<uint32>();
            uint32 ench = fields[1].Get<uint32>();
            float chance = fields[2].Get<float>();

            if (chance > 0.000001f && chance <= 100.0f)
                RandomItemEnch[entry].push_back(EnchStoreItem(ench, chance));

            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> Loaded {} Item Enchantment Definitions in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
        LOG_INFO("server.loading", " ");
    }
    else
    {
        LOG_WARN("server.loading", ">> Loaded 0 Item Enchantment definitions. DB table `item_enchantment_template` is empty.");
        LOG_INFO("server.loading", " ");
    }
}

uint32 GetItemEnchantMod(int32 entry)
{
    if (!entry)
        return 0;

    if (entry == -1)
        return 0;

    EnchantmentStore::const_iterator tab = RandomItemEnch.find(entry);
    if (tab == RandomItemEnch.end())
    {
        LOG_ERROR("sql.sql", "Item RandomProperty / RandomSuffix id #{} used in `item_template` but it does not have records in `item_enchantment_template` table.", entry);
        return 0;
    }

    double dRoll = rand_chance();
    float fCount = 0;

    for (EnchStoreList::const_iterator ench_iter = tab->second.begin(); ench_iter != tab->second.end(); ++ench_iter)
    {
        fCount += ench_iter->chance;

        if (fCount > dRoll)
            return ench_iter->ench;
    }

    //we could get here only if sum of all enchantment chances is lower than 100%
    dRoll = (irand(0, (int)std::floor(fCount * 100) + 1)) / 100;
    fCount = 0;

    for (EnchStoreList::const_iterator ench_iter = tab->second.begin(); ench_iter != tab->second.end(); ++ench_iter)
    {
        fCount += ench_iter->chance;

        if (fCount > dRoll)
            return ench_iter->ench;
    }

    return 0;
}

// ItemEnchantmentMgr.cpp
uint32 GenerateEnchSuffixFactor(uint32 item_id, uint32 customIlvl /*= 0*/)
{
    ItemTemplate const* itemProto = sObjectMgr->GetItemTemplate(item_id);
    if (!itemProto) return 0;

    // ЛОГИКА: Если пришел customIlvl (не 0), используем его. 
    // Если пришел 0, используем стандарт из шаблона (как раньше).
    uint32 effectiveIlvl = (customIlvl != 0) ? customIlvl : itemProto->ItemLevel;

    RandomPropertiesPointsEntry const* randomProperty = sRandomPropertiesPointsStore.LookupEntry(effectiveIlvl);
    if (!randomProperty) return 0;

    uint32 suffixFactor;
    switch (itemProto->InventoryType)
    {
        // Items of that type don`t have points// Предметы, не имеющие характеристик или не являющиеся экипировкой
    case INVTYPE_NON_EQUIP: // Разное (используемые предметы и т.д.)
    case INVTYPE_BAG:       // Сумки
    case INVTYPE_AMMO:      // Боеприпасы
    case INVTYPE_QUIVER:    // Колчаны
        return 0; // Коэффициент 0: статы не генерируются

    // Select point coefficient - крупные предметы с максимальным бюджетом статов
    case INVTYPE_HEAD:      // Голова
    case INVTYPE_SHOULDERS: // Плечи
    case INVTYPE_CHEST:     // Грудь
    case INVTYPE_ROBE:      // Роба
    case INVTYPE_WRISTS:    // Запястья
    case INVTYPE_HANDS:     // Кисти (перчатки)
    case INVTYPE_WAIST:     // Пояс
    case INVTYPE_LEGS:      // Ноги
    case INVTYPE_FEET:      // Ступни
    case INVTYPE_2HWEAPON:  // Двуручное оружие
    case INVTYPE_BODY:      // Рубашки
    case INVTYPE_TABARD:    // Накидки
    case INVTYPE_TRINKET:   // Аксессуары
    case INVTYPE_RELIC:     // Реликвии (идолы, либры и т.д.)
    case INVTYPE_NECK:      // Шея (амулеты)
    case INVTYPE_FINGER:    // Палец (кольца)
    case INVTYPE_SHIELD:    // Щиты
    case INVTYPE_CLOAK:     // Спина (плащи)
    case INVTYPE_HOLDABLE:  // Предметы в левой руке
    case INVTYPE_WEAPON:        // Одноручное оружие (любое)
    case INVTYPE_WEAPONMAINHAND: // Основная рука
    case INVTYPE_WEAPONOFFHAND:  // Вторая рука
    case INVTYPE_RANGED:      // Луки, арбалеты
    case INVTYPE_THROWN:      // Метательное
    case INVTYPE_RANGEDRIGHT: // Огнестрельное / Жезлы
        suffixFactor = 0; // Самый низкий множитель для больших слотов (базовый бюджет)
        break;
        default:
            return 0;
    }
    // Select rare/epic modifier
    switch (itemProto->Quality)
    {
        case ITEM_QUALITY_UNCOMMON:
        case ITEM_QUALITY_RARE:
        case ITEM_QUALITY_EPIC:
        case ITEM_QUALITY_LEGENDARY:
        case ITEM_QUALITY_ARTIFACT:
            return randomProperty->EpicPropertiesPoints[suffixFactor];
        case ITEM_QUALITY_NORMAL: // БЕЛОЕ качество
            // Проверяем: если это рубашка или накидка, даем им статы
            if (itemProto->InventoryType == INVTYPE_BODY || itemProto->InventoryType == INVTYPE_TABARD)
            {
                // Можно использовать EpicPropertiesPoints для крутых статов 
                // или RarePropertiesPoints, если хочешь, чтобы на рубашках статов было чуть меньше
                return randomProperty->EpicPropertiesPoints[suffixFactor];
            }
            break;

        default:
            break;
    }
    return 0;
}
