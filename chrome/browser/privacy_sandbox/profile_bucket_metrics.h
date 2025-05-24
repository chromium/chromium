// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PROFILE_BUCKET_METRICS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PROFILE_BUCKET_METRICS_H_

#include <optional>
#include <string>

class Profile;

namespace privacy_sandbox {
// LINT.IfChange(SettingsPrivacySandboxProfileEnabledState)
enum class ProfileEnabledState {
  kPSProfileOneEnabled = 0,
  kPSProfileOneDisabled = 1,
  kPSProfileTwoEnabled = 2,
  kPSProfileTwoDisabled = 3,
  kPSProfileThreeEnabled = 4,
  kPSProfileThreeDisabled = 5,
  kPSProfileFourEnabled = 6,
  kPSProfileFourDisabled = 7,
  kPSProfileFivePlusEnabled = 8,
  kPSProfileFivePlusDisabled = 9,
  kMaxValue = kPSProfileFivePlusDisabled
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacySandboxProfileEnabledState)

// Returns the profile bucket name associated to a profile used for metrics
// tracking.
std::string GetProfileBucketName(Profile* profile);

// Returns the enabled state of a metric for a given profile.
std::optional<ProfileEnabledState> GetProfileEnabledState(Profile* profile,
                                                          bool enabled);

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PROFILE_BUCKET_METRICS_H_
