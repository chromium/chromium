// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_AUTOCLICK_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_AUTOCLICK_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

// A AutoclickPolicyHandler which sets kAccessibilityAutoclickEnabled, but
// prevents the confirmation popup from showing.
class AutoclickPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  AutoclickPolicyHandler();

  AutoclickPolicyHandler(const AutoclickPolicyHandler&) = delete;
  AutoclickPolicyHandler& operator=(const AutoclickPolicyHandler&) = delete;

  ~AutoclickPolicyHandler() override;

  // TypeCheckingPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_AUTOCLICK_POLICY_HANDLER_H_
