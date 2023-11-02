// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_FORCE_SAFE_SEARCH_POLICY_HANDLER_H_
#define CHROME_BROWSER_PROFILES_FORCE_SAFE_SEARCH_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the deprecated |kForceSafeSearch| policy. Sets both the newer
// |kForceGoogleSafeSearch| and |kForceYouTubeRestrict| prefs, the latter to
// |YOUTUBE_RESTRICT_MODERATE| if set. Does nothing if any of the
// |kForceGoogleSafeSearch|, |kForceYouTubeSafetyMode| and
// |kForceYouTubeRestrict| policies are set because they take precedence.
class ForceSafeSearchPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ForceSafeSearchPolicyHandler();

  ForceSafeSearchPolicyHandler(const ForceSafeSearchPolicyHandler&) = delete;
  ForceSafeSearchPolicyHandler& operator=(const ForceSafeSearchPolicyHandler&) =
      delete;

  ~ForceSafeSearchPolicyHandler() override;

 protected:
  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_PROFILES_FORCE_SAFE_SEARCH_POLICY_HANDLER_H_
