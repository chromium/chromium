// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

AppServicePromiseAppContextMenu::AppServicePromiseAppContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const apps::PackageId& package_id,
    AppListControllerDelegate* controller,
    ash::AppListItemContext item_context)
    : AppContextMenu(delegate,
                     profile,
                     package_id.ToString(),
                     controller,
                     item_context),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)),
      package_id_(package_id) {}

AppServicePromiseAppContextMenu::~AppServicePromiseAppContextMenu() = default;

void AppServicePromiseAppContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  if (!proxy_->PromiseAppRegistryCache()->HasPromiseApp(*package_id_)) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  AddContextMenuOption(menu_model.get(),
                       static_cast<ash::CommandId>(ash::CommandId::TOGGLE_PIN),
                       IDS_APP_LIST_CONTEXT_MENU_PIN);

  AddReorderMenuOption(menu_model.get());
  std::move(callback).Run(std::move(menu_model));
}

void AppServicePromiseAppContextMenu::ExecuteCommand(int command_id,
                                                     int event_flags) {
  AppContextMenu::ExecuteCommand(command_id, event_flags);
}
