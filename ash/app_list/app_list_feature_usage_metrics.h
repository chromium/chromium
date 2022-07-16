// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_FEATURE_USAGE_METRICS_H_
#define ASH_APP_LIST_APP_LIST_FEATURE_USAGE_METRICS_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"

namespace ash {

// Tracks app list feature usage for the Standard Feature Usage Logging
// (go/sful) framework. Tracks clamshell app list and tablet mode app list as
// separate features because the user experience is different and some devices
// are not eligible for tablet mode. Both features are tracked from this single
// class to simplify accounting for a tablet mode transition while the launcher
// is open.
class AppListFeatureUsageMetrics : public TabletModeObserver,
                                   public AppListControllerObserver {
 public:
  AppListFeatureUsageMetrics();
  AppListFeatureUsageMetrics(AppListFeatureUsageMetrics&) = delete;
  AppListFeatureUsageMetrics& operator=(AppListFeatureUsageMetrics&) = delete;
  ~AppListFeatureUsageMetrics() override;

 private:
  // TabletModeObserver:
  void OnTabletModeStarted() override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // Helper methods for app list visibility changes.
  void OnAppListShown();
  void OnAppListHidden();

  // Helper methods to record usage of clamshell and tablet launcher.
  void StartClamshellUsage();
  void StopClamshellUsage();
  void StartTabletUsage();
  void StopTabletUsage();

  std::unique_ptr<feature_usage::FeatureUsageMetrics::Delegate>
      clamshell_delegate_;
  feature_usage::FeatureUsageMetrics clamshell_metrics_;
  bool is_using_clamshell_ = false;

  std::unique_ptr<feature_usage::FeatureUsageMetrics::Delegate>
      tablet_delegate_;
  feature_usage::FeatureUsageMetrics tablet_metrics_;
  bool is_using_tablet_ = false;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_FEATURE_USAGE_METRICS_H_
