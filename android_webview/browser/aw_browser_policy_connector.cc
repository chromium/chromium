// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_policy_connector.h"

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "base/bind.h"
#include "components/policy/core/browser/android/android_combined_policy_provider.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace android_webview {

namespace {

// Callback only used in ChromeOS. No-op here.
void PopulatePolicyHandlerParameters(
    policy::PolicyHandlerParameters* parameters) {}

// Used to check if a policy is deprecated. Currently bypasses that check.
const policy::PolicyDetails* GetChromePolicyDetails(const std::string& policy) {
  return nullptr;
}

// Factory for the handlers that will be responsible for converting the policies
// to the associated preferences.
std::unique_ptr<policy::ConfigurationPolicyHandlerList> BuildHandlerList(
    const policy::Schema& chrome_schema) {
  std::unique_ptr<policy::ConfigurationPolicyHandlerList> handlers(
      new policy::ConfigurationPolicyHandlerList(
          base::BindRepeating(&PopulatePolicyHandlerParameters),
          base::BindRepeating(&GetChromePolicyDetails)));

  // URL Filtering
  handlers->AddHandler(std::make_unique<policy::SimplePolicyHandler>(
      policy::key::kURLWhitelist, policy::policy_prefs::kUrlWhitelist,
      base::Value::Type::LIST));
  handlers->AddHandler(std::make_unique<policy::URLBlacklistPolicyHandler>());

  // HTTP Negotiate authentication
  handlers->AddHandler(std::make_unique<policy::SimplePolicyHandler>(
      policy::key::kAuthServerWhitelist, prefs::kAuthServerWhitelist,
      base::Value::Type::STRING));
  handlers->AddHandler(std::make_unique<policy::SimplePolicyHandler>(
      policy::key::kAuthAndroidNegotiateAccountType,
      prefs::kAuthAndroidNegotiateAccountType, base::Value::Type::STRING));

  return handlers;
}

}  // namespace

AwBrowserPolicyConnector::AwBrowserPolicyConnector()
    : BrowserPolicyConnectorBase(base::BindRepeating(&BuildHandlerList)) {}

AwBrowserPolicyConnector::~AwBrowserPolicyConnector() = default;

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
AwBrowserPolicyConnector::CreatePolicyProviders() {
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>> providers;
  providers.push_back(
      std::make_unique<policy::android::AndroidCombinedPolicyProvider>(
          GetSchemaRegistry()));
  return providers;
}

}  // namespace android_webview
