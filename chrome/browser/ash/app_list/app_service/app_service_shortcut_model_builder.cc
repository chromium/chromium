// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_model_builder.h"
#include <ostream>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_builder.h"
#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_item.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

namespace {
bool ShouldShowInLauncher(const apps::ShortcutSource& shortcut_source) {
  return shortcut_source == apps::ShortcutSource::kUser;
}
}  // namespace

AppServiceShortcutModelBuilder::AppServiceShortcutModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, AppServiceShortcutItem::kItemType) {}

AppServiceShortcutModelBuilder::~AppServiceShortcutModelBuilder() = default;

void AppServiceShortcutModelBuilder::BuildModel() {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  for (auto const& shortcut :
       proxy->ShortcutRegistryCache()->GetAllShortcuts()) {
    if (ShouldShowInLauncher(shortcut->shortcut_source)) {
      auto shortcut_item = std::make_unique<AppServiceShortcutItem>(
          profile(), model_updater(), shortcut);
      InsertApp(std::move(shortcut_item));
    }
  }
  CHECK(!shortcut_registry_cache_observation_.IsObserving());
  shortcut_registry_cache_observation_.Observe(proxy->ShortcutRegistryCache());
}

// Update the App Service Shortcut Item for the appropriate shortcut if
// one already exists. Otherwise, create a new one.
void AppServiceShortcutModelBuilder::OnShortcutUpdated(
    const apps::ShortcutUpdate& update) {
  ChromeAppListItem* item = GetAppItem(update.ShortcutId().value());
  bool show = ShouldShowInLauncher(update.ShortcutSource());
  if (item) {
    if (show) {
      CHECK(item->GetItemType() == AppServiceShortcutItem::kItemType);
      static_cast<AppServiceShortcutItem*>(item)->OnShortcutUpdate(update);
    } else {
      RemoveApp(update.ShortcutId().value(), false);
    }
  } else if (show) {
    auto shortcut_item = std::make_unique<AppServiceShortcutItem>(
        profile(), model_updater(), update);
    InsertApp(std::move(shortcut_item));
  }
}

void AppServiceShortcutModelBuilder::OnShortcutRemoved(
    const apps::ShortcutId& id) {
  RemoveApp(id.value(), false);
}

void AppServiceShortcutModelBuilder::OnShortcutRegistryCacheWillBeDestroyed(
    apps::ShortcutRegistryCache* cache) {
  shortcut_registry_cache_observation_.Reset();
}
