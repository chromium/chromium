// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_FORCE_INSTALLED_APPS_TRACKER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_FORCE_INSTALLED_APPS_TRACKER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/arc/enterprise/arc_apps_tracker.h"

class ArcAppListPrefs;

namespace policy {

class PolicyService;

}  // namespace policy

namespace arc {
namespace data_snapshotd {

class ArcForceInstalledAppsObserver;

// This class tracks ARC apps that are required to be installed by ArcPolicy.
class ArcForceInstalledAppsTracker : public ArcAppsTracker {
 public:
  ArcForceInstalledAppsTracker();
  ArcForceInstalledAppsTracker(const ArcForceInstalledAppsTracker&) = delete;
  ArcForceInstalledAppsTracker& operator=(const ArcForceInstalledAppsTracker&) =
      delete;
  ~ArcForceInstalledAppsTracker() override;

  // Creates instance for testing.
  static std::unique_ptr<ArcForceInstalledAppsTracker> CreateForTesting(
      ArcAppListPrefs* prefs,
      policy::PolicyService* policy_service);

  // ArcAppsTracker overrides:
  void StartTracking(
      base::RepeatingCallback<void(int)> update_callback) override;
  void StopTracking() override;

 private:
  ArcForceInstalledAppsTracker(ArcAppListPrefs* prefs,
                               policy::PolicyService* policy_service);

  // Helper method to initialize |prefs_| and |policy_service_|.
  void Initialize();

  // Not owned singleton. Initialized in StartTracking.
  ArcAppListPrefs* prefs_ = nullptr;
  // Not owned singleton. Initialized in StartTracking.
  policy::PolicyService* policy_service_ = nullptr;

  // Created/destroyed in Start/StopTracking respectively.
  std::unique_ptr<ArcForceInstalledAppsObserver> observer_;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_FORCE_INSTALLED_APPS_TRACKER_H_
