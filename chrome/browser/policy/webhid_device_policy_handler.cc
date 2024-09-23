// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webhid_device_policy_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kUrlsKey[] = "urls";

}  // namespace

WebHidDevicePolicyHandler::WebHidDevicePolicyHandler(
    const char* policy_key,
    std::string_view pref_name,
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          policy_key,
          chrome_schema.GetKnownProperty(policy_key),
          SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY),
      pref_name_(pref_name) {}

WebHidDevicePolicyHandler::~WebHidDevicePolicyHandler() = default;

bool WebHidDevicePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name()))
    return true;
  if (!SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  DCHECK(value);
  int item_index = 0;
  for (const auto& item : value->GetList()) {
    if (!item.is_dict())
      continue;
    int url_index = 0;
    auto* urls_list = item.GetDict().FindList(kUrlsKey);
    if (!urls_list)
      continue;

    for (const auto& url_value : *urls_list) {
      GURL url;
      if (url_value.is_string())
        url = GURL(url_value.GetString());
      // If `url` is invalid, emit an error but do not prevent the policy from
      // being applied.
      if (errors && !url.is_valid()) {
        PolicyErrorPath error_path = {item_index, kUrlsKey, url_index};
        errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR,
                         error_path);
      }
      ++url_index;
    }
    ++item_index;
  }

  return true;
}

void WebHidDevicePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> value;
  if (!CheckAndGetValue(policies, nullptr, &value))
    return;

  if (!value || !value->is_list())
    return;

  prefs->SetValue(pref_name_,
                  base::Value::FromUniquePtrValue(std::move(value)));
}

}  // namespace policy
