// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_item.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_context_menu.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"
#include "ui/gfx/image/image_skia_operations.h"

// static
const char AppServiceShortcutItem::kItemType[] = "AppServiceShortcutItem";

AppServiceShortcutItem::AppServiceShortcutItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::ShortcutUpdate& update,
    const app_list::AppListSyncableService::SyncItem* sync_item)
    : AppServiceShortcutItem(profile,
                             model_updater,
                             update.ShortcutId(),
                             update.Name(),
                             sync_item) {}

AppServiceShortcutItem::AppServiceShortcutItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::ShortcutView& view,
    const app_list::AppListSyncableService::SyncItem* sync_item)
    : AppServiceShortcutItem(profile,
                             model_updater,
                             view->shortcut_id,
                             view->name.value_or(""),
                             sync_item) {}

AppServiceShortcutItem::~AppServiceShortcutItem() = default;

void AppServiceShortcutItem::OnShortcutUpdate(
    const apps::ShortcutUpdate& update) {
  if (update.NameChanged()) {
    SetName(update.Name());
  }
  if (update.IconKeyChanged()) {
    IncrementIconVersion();
  }
}

AppServiceShortcutItem::AppServiceShortcutItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::ShortcutId& shortcut_id,
    const std::string& shortcut_name,
    const app_list::AppListSyncableService::SyncItem* sync_item)
    : ChromeAppListItem(profile, shortcut_id.value()),
      shortcut_id_(shortcut_id) {
  SetName(shortcut_name);
  // TODO(crbug.com/1412708): Consider renaming this interface.
  SetAppStatus(ash::AppStatus::kReady);

  if (sync_item && sync_item->item_ordinal.IsValid()) {
    InitFromSync(sync_item);
  } else {
    SetPosition(CalculateDefaultPositionIfApplicable());
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

const char* AppServiceShortcutItem::GetItemType() const {
  return AppServiceShortcutItem::kItemType;
}

void AppServiceShortcutItem::LoadIcon() {
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->LoadShortcutIconWithBadge(
          shortcut_id_, apps::IconType::kStandard,
          ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
          ash::SharedAppListConfig::instance().shortcut_badge_icon_dimension(),
          /*allow_placeholder_icon = */ false,
          base::BindOnce(&AppServiceShortcutItem::OnLoadIcon,
                         weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceShortcutItem::Activate(int event_flags) {
  int64_t display_id = GetController()->GetAppListDisplayId();
  apps::AppServiceProxyFactory::GetForProfile(profile())->LaunchShortcut(
      apps::ShortcutId(id()), display_id);
}

void AppServiceShortcutItem::GetContextMenuModel(
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<AppServiceShortcutContextMenu>(
      this, profile(), shortcut_id_, GetController(), item_context);
  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* AppServiceShortcutItem::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceShortcutItem::ExecuteLaunchCommand(int event_flags) {
  Activate(event_flags);
}

void AppServiceShortcutItem::OnLoadIcon(apps::IconValuePtr icon_value,
                                        apps::IconValuePtr badge_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard) {
    return;
  }
  if (!badge_value || badge_value->icon_type != apps::IconType::kStandard) {
    return;
  }

  // Temporary put the badge in with existing UI interface for testing purposes.
  // The actual visual will be done in the UI layer with the icon and badge raw
  // icons.
  // TODO(crbug.com/1480423): Remove this when the actual visual is done in the
  // UI.
  if (chromeos::features::IsSeparateWebAppShortcutBadgeIconEnabled()) {
    SetIcon(icon_value->uncompressed, false);
  } else {
    gfx::ImageSkia icon_with_badge =
        gfx::ImageSkiaOperations::CreateIconWithBadge(
            icon_value->uncompressed, badge_value->uncompressed);

    SetIcon(icon_with_badge, icon_value->is_placeholder_icon);
  }
  SetBadgeIcon(badge_value->uncompressed);
}
