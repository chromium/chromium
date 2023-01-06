// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_policy_connector.h"

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/enterprise_authentication_app_link_policy_handler.h"
#include "base/functional/bind.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/channel.h"
#include "net/url_request/url_request_context_getter.h"

namespace android_webview {

namespace {

// Factory for the handlers that will be responsible for converting the policies
// to the associated preferences.
std::unique_ptr<policy::ConfigurationPolicyHandlerList> BuildHandlerList(
    const policy::Schema& chrome_schema) {
  version_info::Channel channel = version_info::android::GetChannel();
  std::unique_ptr<policy::ConfigurationPolicyHandlerList> handlers(
      new policy::ConfigurationPolicyHandlerList(
          policy::ConfigurationPolicyHandlerList::
              PopulatePolicyHandlerParametersCallback(),
          base::BindRepeating(&policy::GetChromePolicyDetails),
          channel != version_info::Channel::STABLE &&
              channel != version_info::Channel::BETA));

  // URL Filtering
  handlers->AddHandler(std::make_unique<policy::SimplePolicyHandler>(
      policy::key::kURLAllowlist, policy::policy_prefs::kUrlAllowlist,
      base::Value::Type::LIST));
  handlers->AddHandler(std::make_unique<policy::URLBlocklistPolicyHandler>(
      policy::key::kURLBlocklist));

  // HTTP Negotiate authentication
  handlers->AddHandler(std::make_unique<policy::SimplePolicyHandler>(
      policy::key::kAuthServerAllowlist, prefs::kAuthServerAllowlist,
      base::Value::Type::STRING));
  handlers->AddHandler(std::make_unique<policy::SimplePolicyHandler>(
      policy::key::kAuthAndroidNegotiateAccountType,
      prefs::kAuthAndroidNegotiateAccountType, base::Value::Type::STRING));

  handlers->AddHandler(
      std::make_unique<policy::EnterpriseAuthenticationAppLinkPolicyHandler>(
          policy::key::kEnterpriseAuthenticationAppLinkPolicy,
          prefs::kEnterpriseAuthAppLinkPolicy));

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
