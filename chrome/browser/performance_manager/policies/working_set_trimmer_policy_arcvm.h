// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_ARCVM_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_ARCVM_H_

#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"
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
      public arc::ArcMetricsService::UserInteractionObserver,
      public arc::ArcSessionManagerObserver,
      public arc::ConnectionObserver<arc::mojom::AppInstance>,
      public arc::ConnectionObserver<arc::mojom::IntentHelperInstance>,
      public wm::ActivationChangeObserver {
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
  static constexpr bool kYesFirstReclaimPostBoot = true;
  static constexpr bool kNotFirstReclaimPostBoot = false;
  mechanism::ArcVmReclaimType IsEligibleForReclaim(
      const base::TimeDelta& arcvm_inactivity_time,
      mechanism::ArcVmReclaimType trim_once_type_after_arcvm_boot,
      bool* is_first_trim_post_boot) override;

  // ArcMetricsService::UserInteractionObserver overrides.
  void OnUserInteraction(arc::UserInteractionType type) override;

  // ArcSessionManagerObserver overrides.
  void OnArcSessionStopped(arc::ArcStopReason stop_reason) override;
  void OnArcSessionRestarting() override;

  // arc::ConnectionObserver<arc::mojom::AppInstance> overrides.
  // arc::ConnectionObserver<arc::mojom::IntentHelperInstance> overrides.
  void OnConnectionReady() override;

  // wm::ActivationChangeObserver overrides.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  static const base::TimeDelta& GetArcVmBootDelayForTesting();

 private:
  friend class base::NoDestructor<WorkingSetTrimmerPolicyArcVm>;
  WorkingSetTrimmerPolicyArcVm();

  void StartObservingUserInteractions();
  void OnConnectionReadyInternal();

  raw_ptr<content::BrowserContext> context_for_testing_ = nullptr;

  // True if ARCVM has already been fully booted and app.mojom connection is
  // established.
  bool is_boot_complete_and_connected_ = false;
  // True if ARCVM window is currently focused.
  bool is_focused_ = false;
  // The time of the last user interacted with ARCVM.
  base::TimeTicks last_user_interaction_;

  // True if IsEligibleForReclaim() has already returned true for the single
  // trim that happens after boot when `trim_once_after_arcvm_boot` is set.
  bool trimmed_at_boot_ = false;
  // True if observing the user's interactions with ARCVM via ArcMetricsService.
  bool observing_user_interactions_ = false;

  base::OneShotTimer timer_;
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_ARCVM_H_
