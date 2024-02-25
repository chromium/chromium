// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_policy_handler.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/re2/src/re2/re2.h"

namespace arc {

namespace {

// Return the first unknown variable in |input|, or |nullopt| if no unknown
// variables exist.
std::optional<std::string_view> FindUnknownVariable(const std::string& input) {
  const std::string variable_matcher = base::StringPrintf(
      "%s|%s|%s|%s|%s|%s|%s", kUserEmail, kUserEmailName, kUserEmailDomain,
      kDeviceDirectoryId, kDeviceSerialNumber, kDeviceAssetId,
      kDeviceAnnotatedLocation);
  const std::string unknown_variable_capture =
      base::StringPrintf("\\$\\{(?:%s)(?::(?:%s))*\\}|(\\$\\{[^}]*?\\})",
                         variable_matcher.c_str(), variable_matcher.c_str());
  const re2::RE2 regex(unknown_variable_capture);
  DCHECK(regex.ok()) << "Error compiling regex: " << regex.error();

  std::string_view capture;
  const bool found_unknown_variable =
      re2::RE2::PartialMatch(input, regex, &capture) &&
      capture.data() != nullptr;

  if (!found_unknown_variable)
    return std::nullopt;

  return capture;
}

// Add warning messages in |arc_policy| for invalid variables in
// |managed_configutation|.
void WarnInvalidVariablesInManagedConfiguration(
    const std::string& application_package_name,
    const base::Value::Dict& managed_configuration,
    policy::PolicyMap::Entry* arc_policy) {
  DCHECK(arc_policy);

  for (const auto kv : managed_configuration) {
    const base::Value& value = kv.second;

    if (value.is_dict()) {
      WarnInvalidVariablesInManagedConfiguration(application_package_name,
                                                 value.GetDict(), arc_policy);
      continue;
    }

    if (!value.is_string())
      continue;

    std::optional<std::string_view> unknown_variable =
        FindUnknownVariable(value.GetString());
    if (!unknown_variable.has_value())
      continue;

    arc_policy->AddMessage(
        policy::PolicyMap::MessageType::kWarning,
        IDS_POLICY_UNKNOWN_ARC_MANAGED_CONFIGURATION_VARIABLE,
        {
            base::UTF8ToUTF16(unknown_variable.value()),
            base::UTF8ToUTF16(application_package_name),
        });
  }
}

}  // namespace

ArcPolicyHandler::ArcPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kArcPolicy,
                                        base::Value::Type::STRING) {}

void ArcPolicyHandler::PrepareForDisplaying(policy::PolicyMap* policies) const {
  const base::Value* value =
      policies->GetValue(policy_name(), base::Value::Type::STRING);
  if (!value)
    return;

  std::optional<base::Value> json = base::JSONReader::Read(
      value->GetString(), base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!json.has_value())
    return;
  const base::Value::Dict& arc_policy = json->GetDict();

  const base::Value::List* apps =
      arc_policy.FindList(policy_util::kArcPolicyKeyApplications);
  if (!apps)
    return;

  for (const base::Value& app_value : *apps) {
    if (!app_value.is_dict())
      continue;
    const base::Value::Dict& application = app_value.GetDict();

    const base::Value::Dict* managed_configuration =
        application.FindDict(ArcPolicyBridge::kManagedConfiguration);
    if (!managed_configuration)
      continue;

    const std::string* package_name =
        application.FindString(ArcPolicyBridge::kPackageName);
    WarnInvalidVariablesInManagedConfiguration(
        package_name ? *package_name : "", *managed_configuration,
        policies->GetMutable(policy_name()));
  }
}

void ArcPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                           PrefValueMap* prefs) {}

}  // namespace arc
