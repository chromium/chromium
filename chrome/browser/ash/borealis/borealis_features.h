// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_

#include <ostream>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"

class Profile;

namespace borealis {

class AsyncHardwareChecker;

class BorealisFeatures {
 public:
  // Enumeration for the reasons borealis might be allowed or not.
  enum class AllowStatus {
    kAllowed,
    kFeatureDisabled,
    kFailedToDetermine,
    kBlockedOnIrregularProfile,
    kBlockedOnNonPrimaryProfile,
    kBlockedOnChildAccount,
    kVmPolicyBlocked,
    kUserPrefBlocked,
    kBlockedByFlag,
    kInsufficientHardware,
  };

  // Creates a per-profile instance of the feature-checker for borealis.
  explicit BorealisFeatures(Profile* profile);

  ~BorealisFeatures();

  // Invokes |callback| with the AllowStatus of borealis, kAllowed means
  // borealis can run, other statuses imply an error.
  void IsAllowed(base::OnceCallback<void(AllowStatus)> callback);

  // Returns true if borealis has been installed and can be run in the profile.
  bool IsEnabled();

 private:
  // Allowedness failures should be from most-unable-to-fix to most fixable.
  // Hence we divide the synchronous checks into pre- and post- hardware.
  AllowStatus PreHardwareChecks();
  AllowStatus PostHardwareChecks();

  void OnHardwareChecked(base::OnceCallback<void(AllowStatus)> callback,
                         base::expected<AllowStatus*, bool> hardware_status);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<AsyncHardwareChecker> async_checker_;
  base::WeakPtrFactory<BorealisFeatures> weak_factory_{this};
};

}  // namespace borealis

std::ostream& operator<<(std::ostream& os,
                         const borealis::BorealisFeatures::AllowStatus& reason);

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_
