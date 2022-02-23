// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_

#include <ostream>
#include <string>

#include "base/callback_forward.h"

class Profile;

namespace borealis {

class AsyncAllowChecker;

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
    kUnsupportedModel,
    kHardwareChecksFailed,
  };

  // Creates a per-profile instance of the feature-checker for borealis.
  explicit BorealisFeatures(Profile* profile);

  ~BorealisFeatures();

  // Invokes |callback| with the AllowStatus of borealis, kAllowed means
  // borealis can run, other statuses imply an error.
  void IsAllowed(base::OnceCallback<void(AllowStatus)> callback);

  // Returns the partial AllowStatus which only performs synchronous checks.
  // Borealis must first pass this check and then the async ones to be truly
  // allowed.
  //
  // This method is useful for systems that need to initialize borealis
  // components before the async checks returns.
  AllowStatus MightBeAllowed();

  // Returns true if borealis has been installed and can be run in the profile.
  bool IsEnabled();

 private:
  Profile* const profile_;
  std::unique_ptr<AsyncAllowChecker> async_checker_;
};

}  // namespace borealis

std::ostream& operator<<(std::ostream& os,
                         const borealis::BorealisFeatures::AllowStatus& reason);

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_
