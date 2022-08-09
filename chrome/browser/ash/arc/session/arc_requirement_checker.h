// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/arc_support_host.h"

class Profile;

namespace arc {

class ArcTermsOfServiceNegotiator;

// ArcRequirementChecker performs necessary checks to make sure that it's OK to
// start ARC for the user.
//
// TODO(hashimoto): Move any ArcSessionManager code related to
//   NEGOTIATING_TERMS_OF_SERVICE and CHECKING_ANDROID_MANAGEMENT into this
//   class. This includes letting this class own ArcAndroidManagementChecker and
//   ArcSupportHost.
class ArcRequirementChecker {
 public:
  ArcRequirementChecker(Profile* profile, ArcSupportHost* support_host);
  ArcRequirementChecker(const ArcRequirementChecker&) = delete;
  const ArcRequirementChecker& operator=(const ArcRequirementChecker&) = delete;
  ~ArcRequirementChecker();

  static void SetUiEnabledForTesting(bool enabled);
  static void SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(bool enabled);

  using BoolCallback = base::OnceCallback<void(bool result)>;
  // Negotiates the terms of service to user.
  void StartTermsOfServiceNegotiation(BoolCallback callback);

 private:
  void OnTermsOfServiceNegotiated(BoolCallback callback, bool accepted);

  Profile* const profile_;
  ArcSupportHost* const support_host_;

  std::unique_ptr<ArcTermsOfServiceNegotiator> terms_of_service_negotiator_;

  base::WeakPtrFactory<ArcRequirementChecker> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_REQUIREMENT_CHECKER_H_
