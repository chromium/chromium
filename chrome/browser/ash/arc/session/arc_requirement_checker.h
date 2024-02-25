// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"
#include "components/policy/core/common/policy_service.h"

class Profile;

namespace arc {

class ArcTermsOfServiceNegotiator;

// ArcRequirementChecker performs necessary checks to make sure that it's OK to
// start ARC for the user.
//
// TODO(hashimoto): Move any ArcSessionManager code related to
//   CHECKING_REQUIREMENTS into this class. This includes letting this class own
//   ArcSupportHost.
class ArcRequirementChecker : public policy::PolicyService::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called to notify that checking of Android management status started
    // during the opt-in flow.
    virtual void OnArcOptInManagementCheckStarted() = 0;
  };

  using AndroidManagementCheckerFactory =
      base::RepeatingCallback<std::unique_ptr<ArcAndroidManagementChecker>(
          Profile* profile,
          bool retry_on_error)>;
  static AndroidManagementCheckerFactory
  GetDefaultAndroidManagementCheckerFactory();

  ArcRequirementChecker(
      Profile* profile,
      ArcSupportHost* support_host,
      AndroidManagementCheckerFactory android_management_checker_factory);
  ArcRequirementChecker(const ArcRequirementChecker&) = delete;
  const ArcRequirementChecker& operator=(const ArcRequirementChecker&) = delete;
  ~ArcRequirementChecker() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static void SetUiEnabledForTesting(bool enabled);
  static void SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(bool enabled);
  static void EnableCheckAndroidManagementForTesting(bool enable);

  // Invokes functions as if requirement checks are completed for testing.
  void EmulateRequirementCheckCompletionForTesting();

  // Starts negotiating the terms of service to user, and checking Android
  // management. This is for first boot case (= Opt-in or OOBE flow case). On a
  // regular boot, use StartBackgroundChecks() instead.
  enum class RequirementCheckResult {
    kOk,
    kTermsOfServicesDeclined,
    kDisallowedByAndroidManagement,
    kAndroidManagementCheckError,
  };
  using StartRequirementChecksCallback =
      base::OnceCallback<void(RequirementCheckResult result)>;
  void StartRequirementChecks(bool is_terms_of_service_negotiation_needed,
                              StartRequirementChecksCallback callback);

  // Starts requirement checks in background (in parallel with starting ARC).
  // This is for a regular boot case.
  enum class BackgroundCheckResult {
    kNoActionRequired,
    kArcShouldBeDisabled,
    kArcShouldBeRestarted,
  };
  using StartBackgroundChecksCallback =
      base::OnceCallback<void(BackgroundCheckResult result)>;
  void StartBackgroundChecks(StartBackgroundChecksCallback callback);

  // policy::PolicyServer::Observer override.
  void OnFirstPoliciesLoaded(policy::PolicyDomain domain) override;

 private:
  enum class State {
    kStopped,
    kNegotiatingTermsOfService,
    kCheckingAndroidManagement,
    kCheckingAndroidManagementBackground,
    kWaitingForPoliciesBackground,
  };

  void OnTermsOfServiceNegotiated(bool accepted);

  void StartAndroidManagementCheck();

  void OnAndroidManagementChecked(
      ArcAndroidManagementChecker::CheckResult result);

  void OnBackgroundAndroidManagementChecked(
      ArcAndroidManagementChecker::CheckResult result);

  // Sets up a timer to wait for policies load, or immediately calls
  // OnFirstPoliciesLoadedOrTimeout.
  void WaitForPoliciesLoad();

  // Called when first policies are loaded or when wait_for_policy_timer_
  // expires.
  void OnFirstPoliciesLoadedOrTimeout();

  const raw_ptr<Profile> profile_;
  const raw_ptr<ArcSupportHost> support_host_;
  const AndroidManagementCheckerFactory android_management_checker_factory_;

  State state_ = State::kStopped;

  std::unique_ptr<ArcTermsOfServiceNegotiator> terms_of_service_negotiator_;
  std::unique_ptr<ArcAndroidManagementChecker> android_management_checker_;

  StartRequirementChecksCallback requirement_check_callback_;
  StartBackgroundChecksCallback background_check_callback_;

  // Timer to wait for policiesin case we are suspecting the user might be
  // transitioning to the managed state.
  base::OneShotTimer wait_for_policy_timer_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ArcRequirementChecker> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_
