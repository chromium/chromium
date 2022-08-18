// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"

class Profile;

namespace arc {

class ArcTermsOfServiceNegotiator;

// ArcRequirementChecker performs necessary checks to make sure that it's OK to
// start ARC for the user.
//
// TODO(hashimoto): Move any ArcSessionManager code related to
//   CHECKING_REQUIREMENTS into this class. This includes letting this class own
//   ArcSupportHost.
class ArcRequirementChecker {
 public:
  // TODO(b/242813462): Make the interface cleaner. (e.g. Using callbacks
  // instead of delegate methods to communicate the check result. Notifying
  // events via an observer interface)
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Called to notify that checking of Android management status started
    // during the opt-in flow.
    virtual void OnArcOptInManagementCheckStarted() = 0;

    // Called when the Android management check is done for
    // StartRequirementChecks().
    virtual void OnAndroidManagementChecked(
        ArcAndroidManagementChecker::CheckResult result) = 0;

    // Called when the background Android management check is done for
    // StartBackgroundAndroidManagementCheck().
    virtual void OnBackgroundAndroidManagementChecked(
        ArcAndroidManagementChecker::CheckResult result) = 0;
  };

  ArcRequirementChecker(Delegate* delegate,
                        Profile* profile,
                        ArcSupportHost* support_host);
  ArcRequirementChecker(const ArcRequirementChecker&) = delete;
  const ArcRequirementChecker& operator=(const ArcRequirementChecker&) = delete;
  ~ArcRequirementChecker();

  static void SetUiEnabledForTesting(bool enabled);
  static void SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(bool enabled);

  // Invokes functions as if requirement checks are completed for testing.
  void EmulateRequirementCheckCompletionForTesting();

  // Starts negotiating the terms of service to user, and checking Android
  // management. This is for first boot case (= Opt-in or OOBE flow case). On a
  // regular boot, use StartBackgroundAndroidManagementCheck() instead.
  void StartRequirementChecks(bool is_terms_of_service_negotiation_needed);

  // Starts Android management check in background (in parallel with starting
  // ARC). This is for secondary or later ARC enabling.
  // The reason running them in parallel is for performance. The secondary or
  // later ARC enabling is typically on "logging into Chrome" for the user who
  // already opted in to use Google Play Store. In such a case, network is
  // typically not yet ready. Thus, if we block ARC boot, it delays several
  // seconds, which is not very user friendly.
  void StartBackgroundAndroidManagementCheck();

 private:
  enum class State {
    kStopped,
    kNegotiatingTermsOfService,
    kCheckingAndroidManagement,
    kCheckingAndroidManagementBackground,
  };

  void OnTermsOfServiceNegotiated(bool accepted);

  void StartAndroidManagementCheck();

  void OnAndroidManagementChecked(
      ArcAndroidManagementChecker::CheckResult result);

  void OnBackgroundAndroidManagementChecked(
      ArcAndroidManagementChecker::CheckResult result);

  Delegate* const delegate_;
  Profile* const profile_;
  ArcSupportHost* const support_host_;

  State state_ = State::kStopped;

  std::unique_ptr<ArcTermsOfServiceNegotiator> terms_of_service_negotiator_;
  std::unique_ptr<ArcAndroidManagementChecker> android_management_checker_;

  base::WeakPtrFactory<ArcRequirementChecker> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_
