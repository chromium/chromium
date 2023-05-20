// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/enterprise_authentication_app_link_policy_handler.h"

#include <memory>

#include "base/strings/string_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {
EnterpriseAuthenticationAppLinkPolicyHandler::
    EnterpriseAuthenticationAppLinkPolicyHandler(const char* policy_name,
                                                 const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path) {}

EnterpriseAuthenticationAppLinkPolicyHandler::
    ~EnterpriseAuthenticationAppLinkPolicyHandler() = default;

bool EnterpriseAuthenticationAppLinkPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return true;
  }

  const base::Value::List& policy_list = value->GetList();
  if (policy_list.empty()) {
    return true;
  }

  // Filters more than |url_util::kMaxFiltersPerPolicy| are ignored, add a
  // warning message.
  if (policy_list.size() > policy::kMaxUrlFiltersPerPolicy) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(policy::kMaxUrlFiltersPerPolicy));
  }

  std::vector<std::string> invalid_policies;
  for (const auto& entry : policy_list) {
    const std::string* url = entry.GetDict().FindString("url");
    if (!url) {
      invalid_policies.push_back(
          "Invalid policy: Required key 'url' does not exists");
    } else if (!ValidatePolicyEntry(url)) {
      invalid_policies.push_back("Invalid url: " + *url);
    }
  }

  if (!invalid_policies.empty()) {
    errors->AddError(policy_name(), IDS_POLICY_PROTO_PARSING_ERROR,
                     base::JoinString(invalid_policies, ","));
  }

  return invalid_policies.size() < policy_list.size();
}

void EnterpriseAuthenticationAppLinkPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value)
    return;

  base::Value::List filtered_values;
  for (const auto& entry : value->GetList()) {
    const std::string* url = entry.GetDict().FindString("url");
    if (ValidatePolicyEntry(url))
      filtered_values.Append(*url);
  }
  if (filtered_values.size() > policy::kMaxUrlFiltersPerPolicy) {
    filtered_values.erase(
        filtered_values.begin() + policy::kMaxUrlFiltersPerPolicy,
        filtered_values.end());
  }

  prefs->SetValue(pref_path_, base::Value(std::move(filtered_values)));
}

// Validates that policy follows official pattern
// https://www.chromium.org/administrators/url-blocklist-filter-format
bool EnterpriseAuthenticationAppLinkPolicyHandler::ValidatePolicyEntry(
    const std::string* policy) {
  url_matcher::util::FilterComponents components;
  return policy && url_matcher::util::FilterToComponents(
                       *policy, &components.scheme, &components.host,
                       &components.match_subdomains, &components.port,
                       &components.path, &components.query);
}

}  // namespace policy
