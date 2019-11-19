// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_PROVIDER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_PROVIDER_H_

#include <map>
#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace gfx {
class Size;
}

namespace ash {
class AppListConfig;
enum class AppListConfigType;

// Used to create and keep track of existing AppListConfigs.
class ASH_PUBLIC_EXPORT AppListConfigProvider {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new config is created. Note that this will not be called
    // for AppListConfigType::kShared configs, as they're assumed to always
    // exist.
    // |config_type| - The created config's type.
    virtual void OnAppListConfigCreated(ash::AppListConfigType config_type) = 0;
  };

  static AppListConfigProvider& Get();

  AppListConfigProvider();
  ~AppListConfigProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets a config for the specified AppListConfigType.
  // If the config does not yet exist, new one will be created if |can_create|
  // is set. Returns nullptr if the config does not exist and cannot be created.
  // NOTE: |can_create| has effect only on config types different than kShared.
  //     A new kShared config will always be created if it does not yet exist.
  AppListConfig* GetConfigForType(ash::AppListConfigType type, bool can_create);

  // Returns the app list config that should be used by an app list instance
  // based on the app list display, and available size for the apps grid.
  // Returns nullptr if the new app list config is the same as |current_config|.
  // |work_area_size|: The work area size of the display showing the app list.
  // |min_horizontal_margin|: The minimum horizontal margins that the apps grid
  //     has to respect (the apps grid width should fit into the space
  //     restricted by these margins).
  // |shelf_height|: The current shelf height.
  // |current_config|: If not null, the app list config currently used by the
  //     app list.
  // TODO(crbug.com/976947): Once ScalableAppList feature is removed (and
  // enabled by default), this should return a reference or a pointer to an
  // AppListConfig owned by |this|, as then the number of possible different
  // configs will be restricted to the number of supported config types.
  std::unique_ptr<AppListConfig> CreateForAppListWidget(
      const gfx::Size& display_work_area_size,
      int min_horizontal_margin,
      int shelf_height,
      const AppListConfig* current_config);

  // Clears the set of configs owned by the provider.
  void ResetForTesting();

 private:
  const AppListConfig& GetBaseConfigForDisplaySize(
      const gfx::Size& display_work_area_size);

  std::map<ash::AppListConfigType, std::unique_ptr<AppListConfig>> configs_;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListConfigProvider);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_PROVIDER_H_
