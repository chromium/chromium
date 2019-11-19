// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_RESOURCES_REMOVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_RESOURCES_REMOVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TickClock;
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace ui {
class Event;
}  // namespace ui

namespace chromeos {

// Handles removal of pre-installed demo mode resources.
// Observes system state to detect when pre-installed demo mode resources are
// not needed anymore, and schedules their deletion from the disk.
//
// Only single instance is expected to be created per process.
//
// Demo mode resources are deleted if the device is not in demo mode, and any
// of the following conditions are satisfied:
//   * device is running low on disk space
//   * device is enrolled in a non-demo-mode domain
//   * enough user activity has been detected on the device
class DemoModeResourcesRemover
    : public CryptohomeClient::Observer,
      public user_manager::UserManager::UserSessionStateObserver,
      public ui::UserActivityObserver {
 public:
  // The reason a removal was requested.
  // DO NOT REORDER - used to report metrics.
  enum class RemovalReason {
    kLowDiskSpace = 0,
    kEnterpriseEnrolled = 1,
    kRegularUsage = 2,
    kMaxValue = kRegularUsage
  };

  // The result of a removal attempt.
  // DO NOT REORDER - used to report metrics.
  enum class RemovalResult {
    // Pre-installed resources were removed.
    kSuccess = 0,

    // Pre-installed resources were not found.
    kNotFound = 1,

    // Pre-installed resources cannot be removed in this session.
    // Not expected to be reported in UMA.
    kNotAllowed = 2,

    // The resources have been previously removed.
    // Not expected to be reported in UMA.
    kAlreadyRemoved = 3,

    // Attempt to remove pre-installed resources failed.
    kFailed = 4,

    kMaxValue = kFailed
  };

  // Callback for a request to remove demo mode resources from the stateful
  // partition.
  using RemovalCallback = base::OnceCallback<void(RemovalResult result)>;

  // Configures how DemoModeResourcesRemover tracks the amount of active
  // device usage in order to determine when demo mode resources are not needed
  // anymore.
  struct UsageAccumulationConfig {
    // Creates the config with default params used in production.
    UsageAccumulationConfig();

    UsageAccumulationConfig(const base::TimeDelta& resources_removal_threshold,
                            const base::TimeDelta& update_interval,
                            const base::TimeDelta& idle_threshold);
    // Amount of accumulated device usage time that warrants demo mode resources
    // removal. When this threshold is reached, demo mode resources removal will
    // be attempted.
    base::TimeDelta resources_removal_threshold;

    // The interval in which accumulated usage time is updated in prefs (during
    // active device usage).
    base::TimeDelta update_interval;

    // The amount of time without user activity after which the device is
    // considered idle.
    base::TimeDelta idle_threshold;
  };

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Gets the demo mode resources remover instance for this process - at most
  // one instance can be created per process.
  static DemoModeResourcesRemover* Get();

  // Creates a demo mode resources remover instance if it's required.
  // It will return nullptr if a DemoModeResourcesRemover is not needed at the
  // time - for example if the resources have previously been removed, or if the
  // device is in demo mode.
  //
  // It should be called at most once per process - creating more than one
  // DemoModeResourcesRemover instances will hit a CHECK.
  static std::unique_ptr<DemoModeResourcesRemover> CreateIfNeeded(
      PrefService* local_state);

  // Method used to determine whether a domain is associated with legacy demo
  // retail mode, where demo mode sessions are implemented as public sessions.
  // Exposed so the matching can be tested.
  // TODO(crbug.com/874778): Remove after legacy retail mode domains have been
  // disabled.
  static bool IsLegacyDemoRetailModeDomain(const std::string& domain);

  ~DemoModeResourcesRemover() override;

  // CryptohomeClient::Observer:
  void LowDiskSpace(uint64_t free_disk_space) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* user) override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // Requests demo mode resources removal from the disk. If a removal operation
  // is already in progress, this method will schedule the callback to be run
  // with the result of the operation in progress.
  void AttemptRemoval(RemovalReason reason, RemovalCallback callback);

  // Allows tests to override the tick clock and configuration for accumulating
  // the amount of active device usage.
  void OverrideTimeForTesting(base::TickClock* tick_clock,
                              const UsageAccumulationConfig& config);

 private:
  // Defined here so it can be overridden in tests.
  UsageAccumulationConfig usage_accumulation_config_;

  // Use CreateIfNeeded() to create an instance.
  explicit DemoModeResourcesRemover(PrefService* local_state);

  // Updates the accumulated information about the amount of active device
  // usage, which is used to detect when the device owned by a real user, and
  // thus does not require demo mode resources.
  void UpdateDeviceUsage(const base::TimeDelta& duration);

  // If the amount of detected device usage is above the threshold for removing
  // demo mode resources, attempts the resources removal. If resoruces removal
  // is requested, stops observing device usage.
  // Returns whether the resources removal was requested.
  bool AttemptRemovalIfUsageOverThreshold();

  // Passes as the callback to directory removal file operations.
  void OnRemovalDone(RemovalReason reason, RemovalResult result);

  PrefService* const local_state_;

  // Whether a resources removal operation is currently in progress.
  bool removal_in_progress_ = false;

  // Callbacks for the resources removal operation, if one is in progress.
  std::vector<RemovalCallback> removal_callbacks_;

  const base::TickClock* tick_clock_;

  // Used to track the duration of last unrecorded interval of user activity.
  base::Optional<base::TimeTicks> usage_start_;
  base::Optional<base::TimeTicks> usage_end_;

  ScopedObserver<CryptohomeClient, CryptohomeClient::Observer>
      cryptohome_observer_{this};
  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_{this};

  base::WeakPtrFactory<DemoModeResourcesRemover> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemover);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_RESOURCES_REMOVER_H_
