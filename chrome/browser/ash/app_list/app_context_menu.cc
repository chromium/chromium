// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace {
void RequestAppListSort(Profile* profile, ash::AppListSortOrder order) {
  ChromeAppListModelUpdater* model_updater =
      static_cast<ChromeAppListModelUpdater*>(
          app_list::AppListSyncableServiceFactory::GetForProfile(profile)
              ->GetModelUpdater());
  model_updater->RequestAppListSort(order);
}
}  // namespace

namespace app_list {

AppContextMenu::AppContextMenu(AppContextMenuDelegate* delegate,
                               Profile* profile,
                               const std::string& app_id,
                               AppListControllerDelegate* controller,
                               ash::AppListItemContext item_context)
    : delegate_(delegate),
      profile_(profile),
      app_id_(app_id),
      controller_(controller),
      item_context_(item_context) {}

AppContextMenu::~AppContextMenu() = default;

void AppContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu(menu_model.get());
  std::move(callback).Run(std::move(menu_model));
}

bool AppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == ash::TOGGLE_PIN;
}

std::u16string AppContextMenu::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(command_id, ash::TOGGLE_PIN);
  // Return "{Pin to, Unpin from} shelf" or "Pinned by administrator".
  // Note this only exists on Ash desktops.
  if (controller_->GetPinnable(app_id()) ==
      AppListControllerDelegate::PIN_FIXED) {
    return l10n_util::GetStringUTF16(
        IDS_APP_LIST_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY);
  }
  return controller_->IsAppPinned(app_id_)
             ? l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN)
             : l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
}

bool AppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::TOGGLE_PIN) {
    return controller_->GetPinnable(app_id_) ==
           AppListControllerDelegate::PIN_EDITABLE;
  }
  return true;
}

void AppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::TOGGLE_PIN:
      TogglePin(app_id_);
      break;
    case ash::REORDER_BY_NAME_ALPHABETICAL:
      RequestAppListSort(profile(), ash::AppListSortOrder::kNameAlphabetical);
      break;
    case ash::REORDER_BY_NAME_REVERSE_ALPHABETICAL:
      RequestAppListSort(profile(),
                         ash::AppListSortOrder::kNameReverseAlphabetical);
      break;
    case ash::REORDER_BY_COLOR:
      RequestAppListSort(profile(), ash::AppListSortOrder::kColor);
      break;
  }
}

ui::ImageModel AppContextMenu::GetIconForCommandId(int command_id) const {
  DCHECK_EQ(command_id, ash::TOGGLE_PIN);
  const gfx::VectorIcon& icon =
      GetMenuItemVectorIcon(command_id, controller_->IsAppPinned(app_id_)
                                            ? IDS_APP_LIST_CONTEXT_MENU_UNPIN
                                            : IDS_APP_LIST_CONTEXT_MENU_PIN);
  return ui::ImageModel::FromVectorIcon(icon, apps::GetColorIdForMenuItemIcon(),
                                        ash::kAppContextMenuIconSize);
}

// static
const gfx::VectorIcon& AppContextMenu::GetMenuItemVectorIcon(int command_id,
                                                             int string_id) {
  switch (command_id) {
    case ash::LAUNCH_NEW:
      if (string_id == IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW)
        return views::kNewWindowIcon;
      if (string_id == IDS_APP_LIST_CONTEXT_MENU_NEW_TAB)
        return views::kNewTabIcon;
      // The LAUNCH_NEW command is for an ARC app.
      return views::kLaunchIcon;
    case ash::TOGGLE_PIN:
      return string_id == IDS_APP_LIST_CONTEXT_MENU_PIN ? views::kPinIcon
                                                        : views::kUnpinIcon;
    case ash::SHOW_APP_INFO:
      return views::kInfoIcon;
    case ash::OPTIONS:
      return views::kOptionsIcon;
    case ash::UNINSTALL:
      return views::kUninstallIcon;
    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      return views::kNewWindowIcon;
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      return views::kNewIncognitoWindowIcon;
    case ash::INSTALL:
      // Deprecated.
      return gfx::kNoneIcon;
    case ash::SETTINGS:
      return vector_icons::kSettingsIcon;
    case ash::USE_LAUNCH_TYPE_REGULAR:
    case ash::USE_LAUNCH_TYPE_WINDOW:
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      // Check items use the default icon.
      return gfx::kNoneIcon;
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      NOTREACHED_IN_MIGRATION();
      return gfx::kNoneIcon;
    case ash::REORDER_SUBMENU:
      return ash::kReorderIcon;
    case ash::REORDER_BY_NAME_ALPHABETICAL:
      return ash::kSortAlphabeticalIcon;
    case ash::REORDER_BY_COLOR:
      return ash::kSortColorIcon;
    case ash::NOTIFICATION_CONTAINER:
      NOTREACHED_IN_MIGRATION()
          << "NOTIFICATION_CONTAINER does not have an icon, and it is "
             "added to the model by NotificationMenuController.";
      return gfx::kNoneIcon;
    case ash::SHUTDOWN_GUEST_OS:
      return kShutdownGuestOsIcon;
    default:
      NOTREACHED_IN_MIGRATION() << "No icon for command_id: " << command_id;
      return gfx::kNoneIcon;
  }
}

void AppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  // Show Pin/Unpin option if shelf is available.
  if (controller_->GetPinnable(app_id()) != AppListControllerDelegate::NO_PIN) {
    AddContextMenuOption(menu_model, ash::TOGGLE_PIN,
                         controller_->IsAppPinned(app_id_)
                             ? IDS_APP_LIST_CONTEXT_MENU_UNPIN
                             : IDS_APP_LIST_CONTEXT_MENU_PIN);
  }
}

void AppContextMenu::TogglePin(const std::string& shelf_app_id) {
  DCHECK_EQ(AppListControllerDelegate::PIN_EDITABLE,
            controller_->GetPinnable(shelf_app_id));
  ash::ShelfModel::ScopedUserTriggeredMutation user_triggered(
      ash::ShelfModel::Get());
  if (controller_->IsAppPinned(shelf_app_id))
    controller_->UnpinApp(shelf_app_id);
  else
    controller_->PinApp(shelf_app_id);
}

void AppContextMenu::AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                                          ash::CommandId command_id,
                                          int string_id) {
  // Do not include disabled items.
  if (!IsCommandIdEnabled(command_id))
    return;

  const gfx::VectorIcon& icon = GetMenuItemVectorIcon(command_id, string_id);
  if (!icon.is_empty()) {
    menu_model->AddItemWithStringIdAndIcon(
        command_id, string_id,
        ui::ImageModel::FromVectorIcon(icon, apps::GetColorIdForMenuItemIcon(),
                                       ash::kAppContextMenuIconSize));
    return;
  }
  // Check items use default icons.
  if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
      command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
    menu_model->AddCheckItemWithStringId(command_id, string_id);
    return;
  }
  if (command_id == ash::NOTIFICATION_CONTAINER) {
    NOTREACHED_IN_MIGRATION()
        << "NOTIFICATION_CONTAINER is added by NotificationMenuController.";
    return;
  }
  menu_model->AddItemWithStringId(command_id, string_id);
}

void AppContextMenu::AddReorderMenuOption(ui::SimpleMenuModel* menu_model) {
  if (item_context_ != ash::AppListItemContext::kAppsGrid &&
      item_context_ != ash::AppListItemContext::kAppsCollectionsGrid) {
    return;
  }
  const ui::ColorId color_id = apps::GetColorIdForMenuItemIcon();
  reorder_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  // As all the options below are only for tests and are expected to change in
  // the future, the strings are directly written as the parameters.
  reorder_submenu_->AddItemWithIcon(
      ash::REORDER_BY_NAME_ALPHABETICAL,
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_REORDER_BY_NAME),
      ui::ImageModel::FromVectorIcon(
          GetMenuItemVectorIcon(ash::REORDER_BY_NAME_ALPHABETICAL,
                                /*string_id=*/-1),
          color_id));
  reorder_submenu_->AddItemWithIcon(
      ash::REORDER_BY_COLOR,
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_REORDER_BY_COLOR),
      ui::ImageModel::FromVectorIcon(
          GetMenuItemVectorIcon(ash::REORDER_BY_COLOR, /*string_id=*/-1),
          color_id));
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);

  menu_model->AddSubMenuWithIcon(
      ash::REORDER_SUBMENU,
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_REORDER_TITLE),
      reorder_submenu_.get(),
      ui::ImageModel::FromVectorIcon(
          GetMenuItemVectorIcon(ash::REORDER_SUBMENU, /*string_id=*/-1),
          color_id));
}

}  // namespace app_list
