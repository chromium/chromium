// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEFAULT_GEOLOCATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEFAULT_GEOLOCATION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

class DefaultGeolocationPolicyHandler : public IntRangePolicyHandlerBase {
 public:
  DefaultGeolocationPolicyHandler();
  DefaultGeolocationPolicyHandler(const DefaultGeolocationPolicyHandler&) =
      delete;
  DefaultGeolocationPolicyHandler& operator=(
      const DefaultGeolocationPolicyHandler&) = delete;
  ~DefaultGeolocationPolicyHandler() override;

  // IntRangePolicyHandlerBase:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEFAULT_GEOLOCATION_POLICY_HANDLER_H_
