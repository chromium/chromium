// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_BADGE_CONTROLLER_H_
#define ASH_APP_LIST_APP_LIST_BADGE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class PrefChangeRegistrar;

namespace ash {

class AppListModel;

// Handles badges on app list items (e.g. notification badges).
class ASH_EXPORT AppListBadgeController
    : public AppListModelProvider::Observer,
      public AppListModelObserver,
      public SessionObserver,
      public apps::AppRegistryCache::Observer {
 public:
  AppListBadgeController();
  AppListBadgeController(const AppListBadgeController&) = delete;
  AppListBadgeController& operator=(const AppListBadgeController&) = delete;
  ~AppListBadgeController() override;

  void Shutdown();

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // AppListModelObserver:
  void OnAppListItemAdded(AppListItem* item) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  // Updates whether a notification badge is shown for the AppListItemView
  // corresponding with the |app_id|.
  void UpdateItemNotificationBadge(const std::string& app_id, bool has_badge);

  // Checks the notification badging pref and then updates whether a
  // notification badge is shown for each AppListItem.
  void UpdateAppNotificationBadging();

  // Sets the active AppListModel and observes it for changes.
  void SetActiveModel(AppListModel* model);

  raw_ptr<AppListModel> model_ = nullptr;

  // Observed to update notification badging on app list items. Also used to get
  // initial notification badge information when app list items are added.
  raw_ptr<apps::AppRegistryCache, DanglingUntriaged> cache_ = nullptr;

  // Observes user profile prefs for the app list.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Whether the pref for notification badging is enabled.
  std::optional<bool> notification_badging_pref_enabled_;

  base::ScopedObservation<AppListModel, AppListModelObserver>
      model_observation_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_BADGE_CONTROLLER_H_
