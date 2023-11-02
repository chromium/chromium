// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_FORCE_YOUTUBE_SAFETY_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_PROFILES_FORCE_YOUTUBE_SAFETY_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the deprecated 2-state |kForceYouTubeSafetyMode| policy. Sets the
// newer 3-state |kForceYouTubeRestrict| pref, mapping |true| to
// |YOUTUBE_RESTRICT_MODERATE| and |false| to |YOUTUBE_RESTRICT_OFF|. Does
// nothing if the |kForceYouTubeRestrict| policy is set because it take
// precedence.
class ForceYouTubeSafetyModePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ForceYouTubeSafetyModePolicyHandler();

  ForceYouTubeSafetyModePolicyHandler(
      const ForceYouTubeSafetyModePolicyHandler&) = delete;
  ForceYouTubeSafetyModePolicyHandler& operator=(
      const ForceYouTubeSafetyModePolicyHandler&) = delete;

  ~ForceYouTubeSafetyModePolicyHandler() override;

 protected:
  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_PROFILES_FORCE_YOUTUBE_SAFETY_MODE_POLICY_HANDLER_H_
