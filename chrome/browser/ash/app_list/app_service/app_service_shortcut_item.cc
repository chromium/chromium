// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_item.h"

#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

// static
const char AppServiceShortcutItem::kItemType[] = "AppServiceShortcutItem";

AppServiceShortcutItem::AppServiceShortcutItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::ShortcutUpdate& update)
    : AppServiceShortcutItem(profile,
                             model_updater,
                             update.ShortcutId(),
                             update.Name()) {}

AppServiceShortcutItem::AppServiceShortcutItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::ShortcutView& view)
    : AppServiceShortcutItem(profile,
                             model_updater,
                             view->shortcut_id,
                             view->name.value_or("")) {}

AppServiceShortcutItem::~AppServiceShortcutItem() = default;

void AppServiceShortcutItem::OnShortcutUpdate(
    const apps::ShortcutUpdate& update) {
  if (update.NameChanged()) {
    SetName(update.Name());
  }
}

AppServiceShortcutItem::AppServiceShortcutItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::ShortcutId& shortcut_id,
    const std::string& shortcut_name)
    : ChromeAppListItem(profile, shortcut_id.value()) {
  SetName(shortcut_name);
  // TODO(crbug.com/1412708): Consider renaming this interface.
  SetAppStatus(ash::AppStatus::kReady);

  SetPosition(CalculateDefaultPositionIfApplicable());

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

const char* AppServiceShortcutItem::GetItemType() const {
  return AppServiceShortcutItem::kItemType;
}
