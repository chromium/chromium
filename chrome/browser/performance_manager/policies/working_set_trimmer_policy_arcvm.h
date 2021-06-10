// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_ARCVM_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_ARCVM_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace performance_manager {
namespace policies {

// A class that implements WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate. Note
// that ALL functions including the constructor and the destructor in the class
// have to be called on the UI thread.
class WorkingSetTrimmerPolicyArcVm
    : public WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate,
      public arc::ArcBootPhaseMonitorBridge::Observer,
      public arc::ArcMetricsService::UserInteractionObserver,
      public arc::ArcSessionManagerObserver,
      public wm::ActivationChangeObserver,
      public session_manager::SessionManagerObserver {
 public:
  // Gets an instance of WorkingSetTrimmerPolicyArcVm.
  static WorkingSetTrimmerPolicyArcVm* Get();

  // Creates the policy with the |context| for testing.
  static std::unique_ptr<WorkingSetTrimmerPolicyArcVm> CreateForTesting(
      content::BrowserContext* context);

  // Creates the policy with the primary profile. Note that ARCVM is only
  // supported on the primary profile.
  WorkingSetTrimmerPolicyArcVm(const WorkingSetTrimmerPolicyArcVm&) = delete;
  WorkingSetTrimmerPolicyArcVm& operator=(const WorkingSetTrimmerPolicyArcVm&) =
      delete;
  ~WorkingSetTrimmerPolicyArcVm() override;

  // WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate overrides.
  bool IsEligibleForReclaim(
      const base::TimeDelta& arcvm_inactivity_time) override;

  // ArcBootPhaseMonitorBridge::Observer overrides.
  void OnBootCompleted() override;

  // ArcMetricsService::UserInteractionObserver overrides.
  void OnUserInteraction(arc::UserInteractionType type) override;

  // ArcSessionManagerObserver overrides.
  void OnArcSessionStopped(arc::ArcStopReason stop_reason) override;
  void OnArcSessionRestarting() override;

  // wm::ActivationChangeObserver overrides.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // session_manager::SessionManagerObserver overrides.
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  friend class base::NoDestructor<WorkingSetTrimmerPolicyArcVm>;
  WorkingSetTrimmerPolicyArcVm();

  content::BrowserContext* context_for_testing_ = nullptr;

  // True if ARCVM has already been fully booted.
  bool is_boot_complete_ = false;
  // True if ARCVM window is currently focused.
  bool is_focused_ = false;
  // The time of the last user interacted with ARCVM.
  base::TimeTicks last_user_interaction_;
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_ARCVM_H_
