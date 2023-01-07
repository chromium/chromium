// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_FORCE_INSTALLED_APPS_TRACKER_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_FORCE_INSTALLED_APPS_TRACKER_H_

#include <memory>

#include "ash/components/arc/enterprise/arc_apps_tracker.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"

class ArcAppListPrefs;

namespace policy {

class PolicyService;

}  // namespace policy

namespace arc {

class ArcPolicyBridge;

namespace data_snapshotd {

class ArcForceInstalledAppsObserver;
class PolicyComplianceObserver;

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
      policy::PolicyService* policy_service,
      arc::ArcPolicyBridge* arc_policy_bridge);

  // ArcAppsTracker overrides:
  void StartTracking(base::RepeatingCallback<void(int)> update_callback,
                     base::OnceClosure finish_callback) override;

 private:
  ArcForceInstalledAppsTracker(ArcAppListPrefs* prefs,
                               policy::PolicyService* policy_service,
                               arc::ArcPolicyBridge* arc_policy_bridge);

  // Helper method to initialize |prefs_| and |policy_service_|.
  void Initialize();

  // Helper method to be invoked once ARC is policy compliant.
  void OnTrackingFinished(base::OnceClosure finish_callback);

  // Not owned singleton. Initialized in StartTracking.
  ArcAppListPrefs* prefs_ = nullptr;
  // Not owned singleton. Initialized in StartTracking.
  policy::PolicyService* policy_service_ = nullptr;
  // Not owned singleton. Initialized in StartTracking.
  arc::ArcPolicyBridge* arc_policy_bridge_ = nullptr;

  // Created in StartTracking, destroyed in OnTrackingFinished or dtor.
  std::unique_ptr<ArcForceInstalledAppsObserver> apps_observer_;
  std::unique_ptr<PolicyComplianceObserver> policy_compliance_observer_;

  base::WeakPtrFactory<ArcForceInstalledAppsTracker> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_FORCE_INSTALLED_APPS_TRACKER_H_
