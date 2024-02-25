// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BATTERY_SAVER_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_BATTERY_SAVER_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Maps the BatterySaverModeAvailability policy, to the battery saver
// controlling prefs in both Chroem and ChromeOS.
class BatterySaverPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  BatterySaverPolicyHandler();
  BatterySaverPolicyHandler(const BatterySaverPolicyHandler&) = delete;
  BatterySaverPolicyHandler& operator=(const BatterySaverPolicyHandler&) =
      delete;
  ~BatterySaverPolicyHandler() override = default;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BATTERY_SAVER_POLICY_HANDLER_H_
