// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREEN_CAPTURE_LOCATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREEN_CAPTURE_LOCATION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the ScreenCaptureLocation policy.
class ScreenCaptureLocationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ScreenCaptureLocationPolicyHandler();
  ScreenCaptureLocationPolicyHandler(
      const ScreenCaptureLocationPolicyHandler&) = delete;
  ScreenCaptureLocationPolicyHandler& operator=(
      const ScreenCaptureLocationPolicyHandler&) = delete;
  ~ScreenCaptureLocationPolicyHandler() override;

  // TypeCheckingPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* error) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREEN_CAPTURE_LOCATION_POLICY_HANDLER_H_
