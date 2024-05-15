// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/menu_util.h"

#include <string_view>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia.h"

namespace {

const int kInvalidRadioGroupId = -1;
const int kGroupId = 1;

apps::MenuItemPtr CreateRadioItem(uint32_t command_id,
                                  uint32_t string_id,
                                  int group_id) {
  apps::MenuItemPtr menu_item =
      std::make_unique<apps::MenuItem>(apps::MenuItemType::kRadio, command_id);
  menu_item->string_id = string_id;
  menu_item->radio_group_id = group_id;
  return menu_item;
}

void PopulateRadioItemFromMenuItems(
    const std::vector<apps::MenuItemPtr>& menu_items,
    ui::SimpleMenuModel* model) {
  for (const auto& item : menu_items) {
    DCHECK_EQ(apps::MenuItemType::kRadio, item->type);
    model->AddRadioItem(item->command_id,
                        l10n_util::GetStringUTF16(item->string_id),
                        item->radio_group_id);
  }
}

}  // namespace

namespace apps {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kLaunchNewMenuItem);

void AddCommandItem(uint32_t command_id,
                    uint32_t string_id,
                    MenuItems& menu_items) {
  MenuItemPtr menu_item =
      std::make_unique<MenuItem>(MenuItemType::kCommand, command_id);
  menu_item->string_id = string_id;
  menu_item->radio_group_id = kInvalidRadioGroupId;
  menu_items.items.push_back(std::move(menu_item));
}

void AddSeparator(ui::MenuSeparatorType separator_type, MenuItems& menu_items) {
  MenuItemPtr menu_item =
      std::make_unique<MenuItem>(MenuItemType::kSeparator, separator_type);
  menu_items.items.push_back(std::move(menu_item));
}

void AddShortcutCommandItem(int command_id,
                            const std::string& shortcut_id,
                            const std::string& label,
                            const gfx::ImageSkia& icon,
                            MenuItems& menu_items) {
  MenuItemPtr menu_item =
      std::make_unique<MenuItem>(MenuItemType::kPublisherCommand, command_id);
  menu_item->shortcut_id = shortcut_id;
  menu_item->label = label;
  menu_item->image = icon;
  menu_items.items.push_back(std::move(menu_item));
}

void CreateOpenNewSubmenu(uint32_t string_id, MenuItems& menu_items) {
  MenuItemPtr menu_item =
      std::make_unique<MenuItem>(MenuItemType::kSubmenu, ash::LAUNCH_NEW);
  menu_item->string_id = string_id;

  menu_item->submenu.push_back(CreateRadioItem(
      ash::USE_LAUNCH_TYPE_REGULAR,
      StringIdForUseLaunchTypeCommand(ash::USE_LAUNCH_TYPE_REGULAR), kGroupId));
  menu_item->submenu.push_back(CreateRadioItem(
      ash::USE_LAUNCH_TYPE_WINDOW,
      StringIdForUseLaunchTypeCommand(ash::USE_LAUNCH_TYPE_WINDOW), kGroupId));
  if (base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip) &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings)) {
    menu_item->submenu.push_back(CreateRadioItem(
        ash::USE_LAUNCH_TYPE_TABBED_WINDOW,
        StringIdForUseLaunchTypeCommand(ash::USE_LAUNCH_TYPE_TABBED_WINDOW),
        kGroupId));
  }

  menu_item->radio_group_id = kInvalidRadioGroupId;

  menu_items.items.push_back(std::move(menu_item));
}

bool ShouldAddOpenItem(const std::string& app_id,
                       MenuType menu_type,
                       Profile* profile) {
  if (menu_type != MenuType::kShelf) {
    return false;
  }

  return !apps::AppServiceProxyFactory::GetForProfile(profile)
              ->InstanceRegistry()
              .ContainsAppId(app_id);
}

bool ShouldAddCloseItem(const std::string& app_id,
                        MenuType menu_type,
                        Profile* profile) {
  if (menu_type != MenuType::kShelf) {
    return false;
  }

  bool can_close = true;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&can_close](const apps::AppUpdate& update) {
        can_close = update.AllowClose().value_or(true);
      });

  return can_close && apps::AppServiceProxyFactory::GetForProfile(profile)
                          ->InstanceRegistry()
                          .ContainsAppId(app_id);
}

void PopulateLaunchNewItemFromMenuItem(const MenuItemPtr& menu_item,
                                       ui::SimpleMenuModel* model,
                                       ui::SimpleMenuModel* submenu,
                                       int* launch_new_string_id) {
  DCHECK_EQ(menu_item->command_id, ash::LAUNCH_NEW);

  if (launch_new_string_id) {
    *launch_new_string_id = menu_item->string_id;
  }

  switch (menu_item->type) {
    case apps::MenuItemType::kCommand: {
      model->AddItemWithStringId(menu_item->command_id, menu_item->string_id);
      model->SetElementIdentifierAt(
          model->GetIndexOfCommandId(menu_item->command_id).value(),
          kLaunchNewMenuItem);
      break;
    }
    case apps::MenuItemType::kSubmenu:
      if (!menu_item->submenu.empty()) {
        PopulateRadioItemFromMenuItems(menu_item->submenu, submenu);
        model->AddActionableSubMenu(
            menu_item->command_id,
            l10n_util::GetStringUTF16(menu_item->string_id), submenu);
      }
      break;
    case apps::MenuItemType::kRadio:
    case apps::MenuItemType::kSeparator:
    case apps::MenuItemType::kPublisherCommand:
      NOTREACHED_IN_MIGRATION();
  }
}

void PopulateItemFromMenuItem(const apps::MenuItemPtr& item,
                              ui::SimpleMenuModel* model,
                              apps::AppShortcutItems* arc_shortcut_items) {
  switch (item->type) {
    case apps::MenuItemType::kSeparator:
      model->AddSeparator(static_cast<ui::MenuSeparatorType>(item->command_id));
      break;
    case apps::MenuItemType::kPublisherCommand: {
      model->AddItemWithIcon(item->command_id, base::UTF8ToUTF16(item->label),
                             ui::ImageModel::FromImageSkia(item->image));
      apps::AppShortcutItem arc_shortcut_item;
      arc_shortcut_item.shortcut_id = item->shortcut_id;
      arc_shortcut_items->push_back(arc_shortcut_item);
      break;
    }
    case apps::MenuItemType::kCommand:
    case apps::MenuItemType::kRadio:
    case apps::MenuItemType::kSubmenu:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

std::string_view MenuTypeToString(MenuType menu_type) {
  switch (menu_type) {
    case MenuType::kShelf:
      return "shelf";
    case MenuType::kAppList:
      return "applist";
  }
}

MenuType MenuTypeFromString(std::string_view menu_type) {
  if (base::EqualsCaseInsensitiveASCII(menu_type, "shelf")) {
    return MenuType::kShelf;
  }
  if (base::EqualsCaseInsensitiveASCII(menu_type, "applist")) {
    return MenuType::kAppList;
  }
  return MenuType::kShelf;
}

MenuItems CreateBrowserMenuItems(const Profile* profile) {
  DCHECK(profile);
  MenuItems menu_items;

  // "Normal" windows are not allowed when incognito is enforced.
  if (IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
      policy::IncognitoModeAvailability::kForced) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW,
                   menu_items);
  }

  // Incognito windows are not allowed when incognito is disabled.
  if (!profile->IsOffTheRecord() &&
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
          policy::IncognitoModeAvailability::kDisabled) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW,
                   IDS_APP_LIST_NEW_INCOGNITO_WINDOW, menu_items);
  }

  AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                 menu_items);

  return menu_items;
}

ui::ColorId GetColorIdForMenuItemIcon() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ui::kColorAshSystemUIMenuIcon;
#else
  return ui::kColorMenuIcon;
#endif
}

uint32_t StringIdForUseLaunchTypeCommand(uint32_t command_id) {
  DCHECK(command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
         command_id < ash::USE_LAUNCH_TYPE_COMMAND_END);
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return IDS_APP_LIST_CONTEXT_MENU_NEW_TAB;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      return IDS_APP_LIST_CONTEXT_MENU_NEW_TABBED_WINDOW;
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      [[fallthrough]];
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

}  // namespace apps
