// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_PROVIDER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_PROVIDER_H_

#include <map>
#include <memory>
#include <set>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace gfx {
class Size;
}

namespace ash {
class AppListConfig;
enum class AppListConfigType;

// Used to create and keep track of existing AppListConfigs.
class ASH_PUBLIC_EXPORT AppListConfigProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a new config is created. Note that this will not be called
    // for AppListConfigType::kShared configs, as they're assumed to always
    // exist.
    // |config_type| - The created config's type.
    virtual void OnAppListConfigCreated(AppListConfigType config_type) = 0;

   protected:
    ~Observer() override = default;
  };

  static AppListConfigProvider& Get();

  AppListConfigProvider();

  AppListConfigProvider(const AppListConfigProvider&) = delete;
  AppListConfigProvider& operator=(const AppListConfigProvider&) = delete;

  ~AppListConfigProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets a config for the specified AppListConfigType.
  // If the config does not yet exist, new one will be created if |can_create|
  // is set. Returns nullptr if the config does not exist and cannot be created.
  // NOTE: |can_create| has effect only on config types different than kShared.
  //     A new kShared config will always be created if it does not yet exist.
  AppListConfig* GetConfigForType(AppListConfigType type, bool can_create);

  // Returns the app list config that should be used by an app list instance
  // based on the app list display, and available size for the apps grid.
  // Returns nullptr if the new app list config is the same as `current_config`.
  // `work_area_size`: The work area size of the display showing the app list.
  // `grid_columns`: The number of columns the root apps grid has. Note that the
  // number of rows will be reduced to fit the grid vertically.

  // `available_size`: The size of the space available for the root apps grid
  // layout.
  // `current_config`: If not null, the app list config currently used by the
  //     app list.
  std::unique_ptr<AppListConfig> CreateForTabletAppList(
      const gfx::Size& display_work_area_size,
      int grid_columns,
      const gfx::Size& available_size,
      const AppListConfig* current_config);

  // Returns all app list config types for which an AppListConfig instance has
  // been created.
  std::set<AppListConfigType> GetAvailableConfigTypes();

  // Clears the set of configs owned by the provider.
  void ResetForTesting();

 private:
  const AppListConfig& GetBaseConfigForDisplaySize(
      const gfx::Size& display_work_area_size);

  std::map<AppListConfigType, std::unique_ptr<AppListConfig>> configs_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_PROVIDER_H_
