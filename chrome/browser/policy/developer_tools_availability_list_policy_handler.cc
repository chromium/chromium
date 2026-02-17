// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_availability_list_policy_handler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"

namespace {
// ValidateHost checks if the given host string is considered valid based on
// the usage of the asterisk character *. An asterisk * as the entire hostname
// is allowed. An asterisk * within any other hostname string (e.g.,
// *.example.com, dev.*.com, example*com) is disallowed and considered
// invalid.
// It is a common mistake that admins allow sites with * as a wildcard
// in the hostname although it has no effect on the domain and subdomains. Two
// example for such a common mistake are: 1- *.android.com 2- developer.*.com
// which allow neither android.com nor developer.android.com
bool ValidateHost(const std::string& host) {
  return host == "*" || host.find('*') == std::string::npos;
}
}  // namespace

namespace policy {

DeveloperToolsAvailabilityListPolicyHandler::
    DeveloperToolsAvailabilityListPolicyHandler(const char* policy_name,
                                                const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path) {}

DeveloperToolsAvailabilityListPolicyHandler::
    ~DeveloperToolsAvailabilityListPolicyHandler() = default;

bool DeveloperToolsAvailabilityListPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name())) {
    return true;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);

  if (!value) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));
    return true;
  }

  // Filters more than |policy::kMaxUrlFiltersPerPolicy| are ignored, add a
  // warning message.
  if (value->GetList().size() > kMaxUrlFiltersPerPolicy) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(kMaxUrlFiltersPerPolicy));
  }

  bool type_error = false;
  std::string policy;
  std::vector<std::string> invalid_policies;
  for (const auto& policy_iter : value->GetList()) {
    if (!policy_iter.is_string()) {
      type_error = true;
      continue;
    }

    policy = policy_iter.GetString();
    if (!ValidatePolicy(policy)) {
      invalid_policies.push_back(policy);
    }
  }

  if (type_error) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
  }

  if (invalid_policies.size()) {
    errors->AddError(policy_name(), IDS_POLICY_PROTO_PARSING_ERROR,
                     base::JoinString(invalid_policies, ","));
  }

  return true;
}

void DeveloperToolsAvailabilityListPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return;
  }

  base::ListValue filtered_list;
  for (const auto& entry : value->GetList()) {
    if (entry.is_string() && ValidatePolicy(entry.GetString())) {
      filtered_list.Append(entry.Clone());
    }
  }

  // Truncate the list of filters if it is too large.
  if (filtered_list.size() > kMaxUrlFiltersPerPolicy) {
    filtered_list.erase(filtered_list.begin() + kMaxUrlFiltersPerPolicy,
                        filtered_list.end());
  }

  if (!filtered_list.empty()) {
    prefs->SetValue(pref_path_, base::Value(std::move(filtered_list)));
  }
}

bool DeveloperToolsAvailabilityListPolicyHandler::ValidatePolicy(
    const std::string& url_pattern) {
  url_matcher::util::FilterComponents components;
  return url_matcher::util::FilterToComponents(
             url_pattern, &components.scheme, &components.host,
             &components.match_subdomains, &components.port, &components.path,
             &components.query) &&
         ValidateHost(components.host);
}

}  // namespace policy
