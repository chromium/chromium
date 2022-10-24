// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_feature_usage_metrics.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {
namespace {

// Feature name for clamshell mode launcher.
constexpr char kClamshellLauncher[] = "ClamshellLauncher";

// Feature name for tablet mode launcher.
constexpr char kTabletLauncher[] = "TabletLauncher";

// Supplies eligibility information for clamshell launcher.
class ClamshellDelegate : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  ClamshellDelegate() = default;
  ClamshellDelegate(const ClamshellDelegate&) = delete;
  ClamshellDelegate& operator=(const ClamshellDelegate&) = delete;
  ~ClamshellDelegate() override = default;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override {
    // Kiosk app sessions cannot use the launcher.
    return !Shell::Get()->session_controller()->IsRunningInAppMode();
  }

  bool IsEnabled() const override {
    // The launcher is always enabled for eligible sessions.
    return IsEligible();
  }
};

// Supplies eligibility information for tablet launcher.
class TabletDelegate : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  TabletDelegate() = default;
  TabletDelegate(const TabletDelegate&) = delete;
  TabletDelegate& operator=(const TabletDelegate&) = delete;
  ~TabletDelegate() override = default;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override {
    // Kiosk app sessions cannot use the launcher.
    if (Shell::Get()->session_controller()->IsRunningInAppMode())
      return false;

    // If the device cannot enter tablet mode, the tablet mode launcher is not
    // available.
    return Shell::Get()->tablet_mode_controller()->CanEnterTabletMode();
  }

  bool IsEnabled() const override {
    // The launcher is always enabled for eligible sessions.
    return IsEligible();
  }
};

}  // namespace

AppListFeatureUsageMetrics::AppListFeatureUsageMetrics()
    : clamshell_delegate_(std::make_unique<ClamshellDelegate>()),
      clamshell_metrics_(kClamshellLauncher, clamshell_delegate_.get()),
      tablet_delegate_(std::make_unique<TabletDelegate>()),
      tablet_metrics_(kTabletLauncher, tablet_delegate_.get()) {
  Shell::Get()->app_list_controller()->AddObserver(this);
}

AppListFeatureUsageMetrics::~AppListFeatureUsageMetrics() {
  Shell::Get()->app_list_controller()->RemoveObserver(this);
}

void AppListFeatureUsageMetrics::OnAppListVisibilityChanged(
    bool shown,
    int64_t display_id) {
  if (shown) {
    OnAppListShown();
  } else {
    OnAppListHidden();
  }
}

void AppListFeatureUsageMetrics::OnAppListShown() {
  if (Shell::Get()->IsInTabletMode()) {
    StartTabletUsage();
  } else {
    StartClamshellUsage();
  }
}

void AppListFeatureUsageMetrics::OnAppListHidden() {
  StopTabletUsage();
  StopClamshellUsage();
}

void AppListFeatureUsageMetrics::StartClamshellUsage() {
  if (is_using_clamshell_)
    return;
  DCHECK(clamshell_delegate_->IsEligible());
  clamshell_metrics_.RecordUsage(true);
  clamshell_metrics_.StartSuccessfulUsage();
  is_using_clamshell_ = true;
}

void AppListFeatureUsageMetrics::StopClamshellUsage() {
  if (!is_using_clamshell_)
    return;
  clamshell_metrics_.StopSuccessfulUsage();
  is_using_clamshell_ = false;
}

void AppListFeatureUsageMetrics::StartTabletUsage() {
  if (is_using_tablet_)
    return;

  // Ignore users on ineligible devices who have forced tablet mode via flags
  // or debug shortcut keys. FeatureUsageMetrics requires that RecordUsage()
  // is never called when IsEligible() is false.
  if (!tablet_delegate_->IsEligible())
    return;

  tablet_metrics_.RecordUsage(true);
  tablet_metrics_.StartSuccessfulUsage();
  is_using_tablet_ = true;
}

void AppListFeatureUsageMetrics::StopTabletUsage() {
  if (!is_using_tablet_)
    return;
  tablet_metrics_.StopSuccessfulUsage();
  is_using_tablet_ = false;
}

}  // namespace ash
