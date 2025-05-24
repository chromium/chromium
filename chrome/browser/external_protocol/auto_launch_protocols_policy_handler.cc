// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/auto_launch_protocols_policy_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/external_protocol/constants.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {

namespace {
const char kValidProtocolChars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-.";

bool IsValidProtocol(std::string_view protocol) {
  // RFC3986: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  if (protocol.empty())
    return false;
  if (!base::IsAsciiAlpha(protocol.front()))
    return false;
  if (protocol.length() > 1 &&
      !base::ContainsOnlyChars(protocol, kValidProtocolChars)) {
    return false;
  }
  return true;
}

// Catches obvious errors like including a [/path] or [@query] element in the
// pattern.
bool IsValidOriginMatchingPattern(std::string_view origin_pattern) {
  GURL gurl(origin_pattern);
  if (gurl.has_path() && gurl.path_piece() != "/")
    return false;
  if (gurl.has_query())
    return false;
  return true;
}

}  // namespace

AutoLaunchProtocolsPolicyHandler::AutoLaunchProtocolsPolicyHandler(
    const policy::Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          policy::key::kAutoLaunchProtocolsFromOrigins,
          chrome_schema.GetKnownProperty(
              policy::key::kAutoLaunchProtocolsFromOrigins),
          policy::SCHEMA_ALLOW_UNKNOWN) {}

AutoLaunchProtocolsPolicyHandler::~AutoLaunchProtocolsPolicyHandler() = default;

bool AutoLaunchProtocolsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value)
    return false;

  base::Value::List& policy_list = policy_value->GetList();
  for (size_t i = 0; i < policy_list.size(); ++i) {
    const base::Value::Dict& protocol_origins_map = policy_list[i].GetDict();

    // If the protocol is invalid mark it as an error.
    const std::string* protocol = protocol_origins_map.FindString(
        policy::external_protocol::kProtocolNameKey);
    DCHECK(protocol);
    if (!IsValidProtocol(*protocol)) {
      errors->AddError(policy::key::kAutoLaunchProtocolsFromOrigins,
                       IDS_POLICY_INVALID_PROTOCOL_ERROR, PolicyErrorPath{i});
    }

    const base::Value::List* origins_list = protocol_origins_map.FindList(
        policy::external_protocol::kOriginListKey);
    for (const auto& entry : *origins_list) {
      const std::string pattern = entry.GetString();
      // If it's not a valid origin pattern mark it as an error.
      if (!IsValidOriginMatchingPattern(pattern)) {
        errors->AddError(policy::key::kAutoLaunchProtocolsFromOrigins,
                         IDS_POLICY_INVALID_ORIGIN_ERROR, PolicyErrorPath{i});
      }
    }
    // If the origin list is empty mark it as an error.
    if (origins_list->empty()) {
      errors->AddError(policy::key::kAutoLaunchProtocolsFromOrigins,
                       IDS_POLICY_EMPTY_ORIGIN_LIST_ERROR, PolicyErrorPath{i});
    }
  }

  // Always continue to ApplyPolicySettings which can remove invalid values and
  // apply the valid ones.
  return true;
}

void AutoLaunchProtocolsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  CheckAndGetValue(policies, nullptr, &policy_value);

  base::Value::List validated_pref_values;
  for (auto& protocol_origins_map : policy_value->GetList()) {
    // If the protocol is invalid skip the entry.
    base::Value::Dict& protocol_origins_dict = protocol_origins_map.GetDict();
    const std::string* protocol = protocol_origins_dict.FindString(
        policy::external_protocol::kProtocolNameKey);
    DCHECK(protocol);
    if (!IsValidProtocol(*protocol))
      continue;

    // Remove invalid patterns from the list.
    base::Value::List* origin_patterns_list = protocol_origins_dict.FindList(
        policy::external_protocol::kOriginListKey);
    origin_patterns_list->EraseIf([](const base::Value& pattern) {
      return !IsValidOriginMatchingPattern(pattern.GetString());
    });
    // If the origin list is empty skip the entry.
    if (origin_patterns_list->size() == 0)
      continue;

    validated_pref_values.Append(protocol_origins_dict.Clone());
  }
  prefs->SetValue(prefs::kAutoLaunchProtocolsFromOrigins,
                  base::Value(std::move(validated_pref_values)));
}

}  // namespace policy
