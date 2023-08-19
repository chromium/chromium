// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_badge_controller.h"

#include <memory>
#include <string>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"

namespace ash {

AppListBadgeController::AppListBadgeController() {
  Shell::Get()->session_controller()->AddObserver(this);

  auto* model_provider = AppListModelProvider::Get();
  DCHECK(model_provider);
  model_provider->AddObserver(this);
  SetActiveModel(model_provider->model());
}

AppListBadgeController::~AppListBadgeController() = default;

void AppListBadgeController::Shutdown() {
  AppListModelProvider::Get()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  model_observation_.Reset();
}

void AppListBadgeController::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  SetActiveModel(model);
}

void AppListBadgeController::OnAppListItemAdded(AppListItem* item) {
  if (cache_ && notification_badging_pref_enabled_.value_or(false)) {
    // Update the notification badge indicator for the newly added app list
    // item.
    cache_->ForOneApp(item->id(), [item](const apps::AppUpdate& update) {
      item->UpdateNotificationBadge(update.HasBadge().value_or(false));
    });
  }
}

void AppListBadgeController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  pref_change_registrar_->Add(
      prefs::kAppNotificationBadgingEnabled,
      base::BindRepeating(&AppListBadgeController::UpdateAppNotificationBadging,
                          base::Unretained(this)));

  // Observe AppRegistryCache for the current active account to get
  // notification updates.
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  cache_ = apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);

  app_registry_cache_observer_.Reset();
  if (cache_) {
    app_registry_cache_observer_.Observe(cache_);
  }

  // Resetting the recorded pref forces the next call to
  // UpdateAppNotificationBadging() to update notification badging for every
  // app item.
  notification_badging_pref_enabled_.reset();
  UpdateAppNotificationBadging();
}

void AppListBadgeController::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.HasBadgeChanged() &&
      notification_badging_pref_enabled_.value_or(false)) {
    UpdateItemNotificationBadge(update.AppId(),
                                update.HasBadge().value_or(false));
  }
}

void AppListBadgeController::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppListBadgeController::UpdateItemNotificationBadge(
    const std::string& app_id,
    bool has_badge) {
  if (!model_)
    return;
  AppListItem* item = model_->FindItem(app_id);
  if (!item)
    return;
  item->UpdateNotificationBadge(has_badge);
}

void AppListBadgeController::UpdateAppNotificationBadging() {
  bool new_badging_enabled = pref_change_registrar_
                                 ? pref_change_registrar_->prefs()->GetBoolean(
                                       prefs::kAppNotificationBadgingEnabled)
                                 : false;

  if (notification_badging_pref_enabled_.has_value() &&
      notification_badging_pref_enabled_.value() == new_badging_enabled) {
    return;
  }
  notification_badging_pref_enabled_ = new_badging_enabled;

  if (cache_) {
    cache_->ForEachApp([this](const apps::AppUpdate& update) {
      // Set the app notification badge hidden when the pref is disabled.
      bool has_badge = notification_badging_pref_enabled_.value() &&
                       (update.HasBadge().value_or(false));
      UpdateItemNotificationBadge(update.AppId(), has_badge);
    });
  }
}

void AppListBadgeController::SetActiveModel(AppListModel* model) {
  model_ = model;
  model_observation_.Reset();

  if (model_)
    model_observation_.Observe(model_.get());
}

}  // namespace ash
