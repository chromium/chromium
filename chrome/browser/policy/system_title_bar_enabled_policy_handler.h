// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SYSTEM_TITLE_BAR_ENABLED_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_SYSTEM_TITLE_BAR_ENABLED_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Maps SystemTitleBarEnabled to the inverse custom-frame preference.
class SystemTitleBarEnabledPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SystemTitleBarEnabledPolicyHandler();
  ~SystemTitleBarEnabledPolicyHandler() override;

 protected:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SYSTEM_TITLE_BAR_ENABLED_POLICY_HANDLER_H_
