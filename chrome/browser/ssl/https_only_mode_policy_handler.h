// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Checks and converts the strings in policy::key::kHttpsOnlyMode to the boolean
// pref::kHttpsOnlyModeEnabled. This currently only sets the associated pref to
// `false` if the policy is set to "disallowed". Otherwise, the policy has no
// effect.
class HttpsOnlyModePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  HttpsOnlyModePolicyHandler();
  ~HttpsOnlyModePolicyHandler() override;
  HttpsOnlyModePolicyHandler(const HttpsOnlyModePolicyHandler&) = delete;
  HttpsOnlyModePolicyHandler& operator=(const HttpsOnlyModePolicyHandler&) =
      delete;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_POLICY_HANDLER_H_
