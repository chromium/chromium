// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_

#include <ostream>
#include <string>

class Profile;

namespace borealis {

class BorealisFeatures {
 public:
  // Enumeration for the reasons borealis might be allowed or not.
  enum class AllowStatus {
    kAllowed,
    kFeatureDisabled,
    kDevicePolicyBlocked,
    kUserPrefBlocked,
  };

  // Static accessor for the allowed-ness check. Returns an AllowStatus
  // relevant to the given |profile|.
  static AllowStatus GetAllowanceForProfile(Profile* profile);

  // Creates a per-profile instance of the feature-checker for borealis.
  explicit BorealisFeatures(Profile* profile);

  // Returns true if borealis can be installed on the profile associated with
  // this feature check.
  bool IsAllowed();

  // Returns true if borealis has been installed and can be run in the profile.
  bool IsEnabled();

 private:
  Profile* const profile_;
};

}  // namespace borealis

std::ostream& operator<<(std::ostream& os,
                         const borealis::BorealisFeatures::AllowStatus& reason);

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_H_
