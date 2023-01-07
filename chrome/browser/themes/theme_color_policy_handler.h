// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_COLOR_POLICY_HANDLER_H_
#define CHROME_BROWSER_THEMES_THEME_COLOR_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

// ConfigurationPolicyHandler for the BrowserThemeColor policy.
class ThemeColorPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  ThemeColorPolicyHandler();
  ThemeColorPolicyHandler(const ThemeColorPolicyHandler&) = delete;
  ThemeColorPolicyHandler& operator=(const ThemeColorPolicyHandler&) = delete;
  ~ThemeColorPolicyHandler() override;

  // policy::TypeCheckingPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // CHROME_BROWSER_THEMES_THEME_COLOR_POLICY_HANDLER_H_
