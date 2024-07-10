// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_METRICS_UTILS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_METRICS_UTILS_H_

namespace ash::on_device_controls {

inline constexpr char kOnDeviceControlsAppRemovalHistogramName[] =
    "ChromeOS.OnDeviceControls.AppRemoval";
inline constexpr char kOnDeviceControlsBlockAppActionHistogramName[] =
    "ChromeOS.OnDeviceControls.BlockAppAction";
inline constexpr char kOnDeviceControlsBlockedAppsCountHistogramName[] =
    "ChromeOS.OnDeviceControls.BlockedAppsCount";
inline constexpr char kOnDeviceControlsBlockedAppsEngagementHistogramName[] =
    "ChromeOS.OnDeviceControls.BlockedAppsEngagement";
inline constexpr char kOnDeviceControlsPinSetCompletedHistogramName[] =
    "ChromeOS.OnDeviceControls.PinSetupCompleted";

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "OnDeviceControlsAppRemoval" in
// src/tools/metrics/histograms/metadata/families/enums.xml.
enum class OnDeviceControlsAppRemoval {
  kOldestUninstalledAppRemoved = 0,
  kOldestUninstalledAppNotFound = 1,
  kMaxValue = kOldestUninstalledAppNotFound,
};

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "OnDeviceControlsBlockAppAction" in
// src/tools/metrics/histograms/metadata/families/enums.xml.
enum class OnDeviceControlsBlockAppAction {
  kBlockApp = 0,
  kUnblockApp = 1,
  kUninstallBlockedApp = 2,
  kBlockAppError = 3,
  kUnblockAppError = 4,
  kUnblockAllApps = 5,
  kMaxValue = kUnblockAllApps,
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_METRICS_UTILS_H_
