// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_context_menu.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

AppServiceShortcutContextMenu::AppServiceShortcutContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const apps::ShortcutId& shortcut_id,
    AppListControllerDelegate* controller,
    ash::AppListItemContext item_context)
    : AppContextMenu(delegate,
                     profile,
                     shortcut_id.value(),
                     controller,
                     item_context),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)),
      shortcut_id_(shortcut_id) {}

AppServiceShortcutContextMenu::~AppServiceShortcutContextMenu() = default;

void AppServiceShortcutContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  if (!proxy_->ShortcutRegistryCache()->HasShortcut(shortcut_id_)) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);

  // Open shortcut in its host app.
  apps::ShortcutView shortcut =
      proxy_->ShortcutRegistryCache()->GetShortcut(shortcut_id_);
  std::u16string shortcut_name = base::UTF8ToUTF16(shortcut->name.value());
  std::u16string host_app_name;
  proxy_->AppRegistryCache().ForOneApp(
      shortcut->host_app_id, [&host_app_name](const apps::AppUpdate& update) {
        host_app_name = base::UTF8ToUTF16(update.ShortName());
      });

  std::u16string open_label = l10n_util::GetStringFUTF16(
      IDS_APP_CONTEXT_MENU_LAUNCH_APP_SHORTCUT, shortcut_name, host_app_name);
  menu_model->AddItemWithIcon(
      ash::LAUNCH_NEW, open_label,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorAshSystemUIMenuIcon,
                                     ash::kAppContextMenuIconSize));

  AddContextMenuOption(menu_model.get(),
                       static_cast<ash::CommandId>(ash::CommandId::TOGGLE_PIN),
                       IDS_APP_LIST_CONTEXT_MENU_PIN);

  if (shortcut->allow_removal.value_or(true)) {
    AddContextMenuOption(menu_model.get(),
                         static_cast<ash::CommandId>(ash::UNINSTALL),
                         IDS_APP_LIST_REMOVE_SHORTCUT);
  }

  AddReorderMenuOption(menu_model.get());

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShortcutContextMenu::ExecuteCommand(int command_id,
                                                   int event_flags) {
  switch (command_id) {
    case ash::LAUNCH_NEW:
      delegate()->ExecuteLaunchCommand(event_flags);
      break;
    case ash::UNINSTALL:
      proxy_->RemoveShortcut(shortcut_id_, apps::UninstallSource::kAppList,
                             controller()->GetAppListWindow());
      break;
    default:
      AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}
