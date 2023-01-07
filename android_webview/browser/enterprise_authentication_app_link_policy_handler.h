// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_HANDLER_H_

#include "android_webview/browser/aw_browser_process.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/browser/url_allowlist_policy_handler.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/url_matcher/url_util.h"

namespace policy {

// Policy handler for EnterpriseAuthenticationAppLink policy
class EnterpriseAuthenticationAppLinkPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  EnterpriseAuthenticationAppLinkPolicyHandler(const char* policy_name,
                                               const char* pref_path);

  EnterpriseAuthenticationAppLinkPolicyHandler(
      const EnterpriseAuthenticationAppLinkPolicyHandler&) = delete;
  EnterpriseAuthenticationAppLinkPolicyHandler& operator=(
      const EnterpriseAuthenticationAppLinkPolicyHandler&) = delete;
  ~EnterpriseAuthenticationAppLinkPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool ValidatePolicyEntry(const std::string* policy);
  const char* pref_path_;
};

}  // namespace policy

#endif  // ANDROID_WEBVIEW_BROWSER_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_HANDLER_H_
