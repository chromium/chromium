// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webhid_device_policy_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kUrlsKey[] = "urls";

}  // namespace

WebHidDevicePolicyHandler::WebHidDevicePolicyHandler(
    const char* policy_key,
    base::StringPiece pref_name,
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          policy_key,
          chrome_schema.GetKnownProperty(policy_key),
          SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY),
      pref_name_(pref_name) {}

WebHidDevicePolicyHandler::~WebHidDevicePolicyHandler() = default;

bool WebHidDevicePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (!value)
    return true;

  if (!SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  int item_index = 0;
  for (const auto& item : value->GetListDeprecated()) {
    int url_index = 0;
    auto* urls_list = item.FindKeyOfType(kUrlsKey, base::Value::Type::LIST);
    if (!urls_list)
      continue;

    for (const auto& url_value : urls_list->GetListDeprecated()) {
      DCHECK(url_value.is_string());
      GURL url(url_value.GetString());
      // If `url` is invalid, emit an error but do not prevent the policy from
      // being applied.
      if (errors && !url.is_valid()) {
        std::string error_path = base::StringPrintf(
            "items[%d].%s.items[%d]", item_index, kUrlsKey, url_index);
        std::string error = base::StringPrintf("Invalid URL: %s",
                                               url_value.GetString().c_str());
        errors->AddError(policy_name(), error_path, error);
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
