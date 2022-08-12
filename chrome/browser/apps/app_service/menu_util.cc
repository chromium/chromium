// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/menu_util.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia.h"

namespace {
const int kInvalidRadioGroupId = -1;
const int kGroupId = 1;
}

namespace apps {

void AddCommandItem(uint32_t command_id,
                    uint32_t string_id,
                    apps::mojom::MenuItemsPtr* menu_items) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kCommand;
  menu_item->command_id = command_id;
  menu_item->string_id = string_id;
  menu_item->radio_group_id = kInvalidRadioGroupId;
  (*menu_items)->items.push_back(std::move(menu_item));
}

apps::mojom::MenuItemPtr CreateRadioItem(uint32_t command_id,
                                         uint32_t string_id,
                                         int group_id) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kRadio;
  menu_item->command_id = command_id;
  menu_item->string_id = string_id;
  menu_item->radio_group_id = group_id;
  return menu_item;
}

void AddRadioItem(uint32_t command_id,
                  uint32_t string_id,
                  int group_id,
                  apps::mojom::MenuItemsPtr* menu_items) {
  (*menu_items)
      ->items.push_back(CreateRadioItem(command_id, string_id, group_id));
}

void AddSeparator(ui::MenuSeparatorType separator_type,
                  apps::mojom::MenuItemsPtr* menu_items) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kSeparator;
  menu_item->command_id = separator_type;
  (*menu_items)->items.push_back(std::move(menu_item));
}

void AddShortcutCommandItem(int command_id,
                            const std::string& shortcut_id,
                            const std::string& label,
                            const gfx::ImageSkia& icon,
                            apps::mojom::MenuItemsPtr* menu_items) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kPublisherCommand;
  menu_item->command_id = command_id;
  menu_item->shortcut_id = shortcut_id;
  menu_item->label = label;
  menu_item->image = icon;
  (*menu_items)->items.push_back(std::move(menu_item));
}

void CreateOpenNewSubmenu(uint32_t string_id,
                          apps::mojom::MenuItemsPtr* menu_items) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kSubmenu;
  menu_item->command_id = ash::LAUNCH_NEW;
  menu_item->string_id = string_id;

  menu_item->submenu.push_back(CreateRadioItem(
      ash::USE_LAUNCH_TYPE_REGULAR,
      StringIdForUseLaunchTypeCommand(ash::USE_LAUNCH_TYPE_REGULAR), kGroupId));
  menu_item->submenu.push_back(CreateRadioItem(
      ash::USE_LAUNCH_TYPE_WINDOW,
      StringIdForUseLaunchTypeCommand(ash::USE_LAUNCH_TYPE_WINDOW), kGroupId));
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip) &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings)) {
    menu_item->submenu.push_back(CreateRadioItem(
        ash::USE_LAUNCH_TYPE_TABBED_WINDOW,
        StringIdForUseLaunchTypeCommand(ash::USE_LAUNCH_TYPE_TABBED_WINDOW),
        kGroupId));
  }

  menu_item->radio_group_id = kInvalidRadioGroupId;

  (*menu_items)->items.push_back(std::move(menu_item));
}

bool ShouldAddOpenItem(const std::string& app_id,
                       apps::mojom::MenuType menu_type,
                       Profile* profile) {
  if (menu_type != apps::mojom::MenuType::kShelf) {
    return false;
  }

  return !apps::AppServiceProxyFactory::GetForProfile(profile)
              ->InstanceRegistry()
              .ContainsAppId(app_id);
}

bool ShouldAddCloseItem(const std::string& app_id,
                        apps::mojom::MenuType menu_type,
                        Profile* profile) {
  if (menu_type != apps::mojom::MenuType::kShelf) {
    return false;
  }

  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->InstanceRegistry()
      .ContainsAppId(app_id);
}

void PopulateRadioItemFromMojoMenuItems(
    const std::vector<apps::mojom::MenuItemPtr>& menu_items,
    ui::SimpleMenuModel* model) {
  for (auto& item : menu_items) {
    DCHECK_EQ(apps::mojom::MenuItemType::kRadio, item->type);
    model->AddRadioItem(item->command_id,
                        l10n_util::GetStringUTF16(item->string_id),
                        item->radio_group_id);
  }
}

bool PopulateNewItemFromMojoMenuItems(
    const std::vector<apps::mojom::MenuItemPtr>& menu_items,
    ui::SimpleMenuModel* model,
    ui::SimpleMenuModel* submenu,
    int* launch_new_string_id) {
  if (menu_items.empty()) {
    return false;
  }

  auto& item = menu_items[0];
  if (item->command_id != ash::LAUNCH_NEW)
    return false;

  if (launch_new_string_id)
    *launch_new_string_id = item->string_id;

  switch (item->type) {
    case apps::mojom::MenuItemType::kCommand: {
      model->AddItemWithStringId(item->command_id, item->string_id);
      break;
    }
    case apps::mojom::MenuItemType::kSubmenu:
      if (!item->submenu.empty()) {
        PopulateRadioItemFromMojoMenuItems(item->submenu, submenu);
        model->AddActionableSubMenu(item->command_id,
                                    l10n_util::GetStringUTF16(item->string_id),
                                    submenu);
      }
      break;
    case apps::mojom::MenuItemType::kRadio:
    case apps::mojom::MenuItemType::kSeparator:
    case apps::mojom::MenuItemType::kPublisherCommand:
      NOTREACHED();
      return false;
  }
  return true;
}

void PopulateItemFromMojoMenuItems(apps::mojom::MenuItemPtr item,
                                   ui::SimpleMenuModel* model,
                                   apps::AppShortcutItems* arc_shortcut_items) {
  switch (item->type) {
    case apps::mojom::MenuItemType::kSeparator:
      model->AddSeparator(static_cast<ui::MenuSeparatorType>(item->command_id));
      break;
    case apps::mojom::MenuItemType::kPublisherCommand: {
      model->AddItemWithIcon(item->command_id, base::UTF8ToUTF16(item->label),
                             ui::ImageModel::FromImageSkia(item->image));
      apps::AppShortcutItem arc_shortcut_item;
      arc_shortcut_item.shortcut_id = item->shortcut_id;
      arc_shortcut_items->push_back(arc_shortcut_item);
      break;
    }
    case apps::mojom::MenuItemType::kCommand:
    case apps::mojom::MenuItemType::kRadio:
    case apps::mojom::MenuItemType::kSubmenu:
      NOTREACHED();
      break;
  }
}

base::StringPiece MenuTypeToString(apps::mojom::MenuType menu_type) {
  switch (menu_type) {
    case apps::mojom::MenuType::kShelf:
      return "shelf";
    case apps::mojom::MenuType::kAppList:
      return "applist";
  }
}

apps::mojom::MenuType MenuTypeFromString(base::StringPiece menu_type) {
  if (base::EqualsCaseInsensitiveASCII(menu_type, "shelf"))
    return apps::mojom::MenuType::kShelf;
  if (base::EqualsCaseInsensitiveASCII(menu_type, "applist"))
    return apps::mojom::MenuType::kAppList;
  return apps::mojom::MenuType::kShelf;
}

mojom::MenuItemsPtr CreateBrowserMenuItems(const Profile* profile) {
  DCHECK(profile);
  mojom::MenuItemsPtr menu_items = mojom::MenuItems::New();

  // "Normal" windows are not allowed when incognito is enforced.
  if (IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
      IncognitoModePrefs::Availability::kForced) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW,
                   &menu_items);
  }

  // Incognito windows are not allowed when incognito is disabled.
  if (!profile->IsOffTheRecord() &&
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
          IncognitoModePrefs::Availability::kDisabled) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW,
                   IDS_APP_LIST_NEW_INCOGNITO_WINDOW, &menu_items);
  }

  AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                 &menu_items);

  return menu_items;
}

ui::ColorId GetColorIdForMenuItemIcon() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ui::kColorAshSystemUIMenuIcon;
#else
  return ui::kColorMenuIcon;
#endif
}

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
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
    default:
      NOTREACHED();
      return 0;
  }
}

extensions::LaunchType ConvertLaunchTypeCommandToExtensionLaunchType(
    uint32_t command_id) {
  DCHECK(command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
         command_id < ash::USE_LAUNCH_TYPE_COMMAND_END);
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_PINNED:
      return extensions::LAUNCH_TYPE_PINNED;
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return extensions::LAUNCH_TYPE_REGULAR;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return extensions::LAUNCH_TYPE_WINDOW;
    case ash::USE_LAUNCH_TYPE_FULLSCREEN:
      return extensions::LAUNCH_TYPE_FULLSCREEN;
    default:
      return extensions::LAUNCH_TYPE_INVALID;
  }
}

apps::WindowMode ConvertLaunchTypeCommandToWindowMode(uint32_t command_id) {
  DCHECK(command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
         command_id < ash::USE_LAUNCH_TYPE_COMMAND_END);
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return apps::WindowMode::kBrowser;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return apps::WindowMode::kWindow;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      return apps::WindowMode::kTabbedWindow;
    default:
      return apps::WindowMode::kUnknown;
  }
}

}  // namespace apps
