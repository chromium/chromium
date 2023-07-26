// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_context_menu.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

void RequestAppListSort(Profile* profile, ash::AppListSortOrder order) {
  ChromeAppListModelUpdater* model_updater =
      static_cast<ChromeAppListModelUpdater*>(
          app_list::AppListSyncableServiceFactory::GetForProfile(profile)
              ->GetModelUpdater());
  model_updater->RequestAppListSort(order);
}
}  // namespace

AppServicePromiseAppContextMenu::AppServicePromiseAppContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const apps::PackageId& package_id,
    AppListControllerDelegate* controller,
    ash::AppListItemContext item_context)
    : AppContextMenu(delegate, profile, package_id.ToString(), controller),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)),
      item_context_(item_context),
      package_id_(package_id) {}

AppServicePromiseAppContextMenu::~AppServicePromiseAppContextMenu() = default;

void AppServicePromiseAppContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  if (!proxy_->PromiseAppRegistryCache()->HasPromiseApp(package_id_)) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  AddContextMenuOption(menu_model.get(),
                       static_cast<ash::CommandId>(ash::CommandId::TOGGLE_PIN),
                       IDS_APP_LIST_CONTEXT_MENU_PIN);

  if (item_context_ == ash::AppListItemContext::kAppsGrid) {
    reorder_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
    const ui::ColorId color_id = apps::GetColorIdForMenuItemIcon();

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
  std::move(callback).Run(std::move(menu_model));
}

void AppServicePromiseAppContextMenu::ExecuteCommand(int command_id,
                                                     int event_flags) {
  switch (command_id) {
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
    default:
      AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}
