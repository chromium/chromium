// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace bruschetta {

BruschettaInstallerPolicyHandler::BruschettaInstallerPolicyHandler(
    policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kBruschettaInstallerConfiguration,
          prefs::kBruschettaInstallerConfiguration,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          policy::SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          policy::SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

BruschettaInstallerPolicyHandler::~BruschettaInstallerPolicyHandler() = default;

bool BruschettaInstallerPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Delegate to our super-class, which checks the JSON schema.
  if (!policy::SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(
          policies, errors)) {
    return false;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);
  if (!value) {
    return true;
  }

  const auto* url = value->GetDict().FindString(prefs::kPolicyLearnMoreUrlKey);
  if (!url) {
    return true;
  }

  GURL g{*url};
  if (!g.is_valid()) {
    errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR,
                     policy::PolicyErrorPath{prefs::kPolicyLearnMoreUrlKey},
                     policy::PolicyMap::MessageType::kError);
    return false;
  }
  if (!g.SchemeIs("https")) {
    errors->AddError(policy_name(), IDS_POLICY_URL_NOT_HTTPS_ERROR,
                     policy::PolicyErrorPath{prefs::kPolicyLearnMoreUrlKey},
                     policy::PolicyMap::MessageType::kError);
    return false;
  }
  return true;
}

void BruschettaInstallerPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);
  if (!value) {
    return;
  }

  base::Value::Dict pref;
  auto& dict = value->GetDict();
  const auto* display_name = dict.FindString(prefs::kPolicyDisplayNameKey);
  if (display_name) {
    pref.Set(prefs::kPolicyDisplayNameKey, *display_name);
  }
  const auto* learn_more_url = dict.FindString(prefs::kPolicyLearnMoreUrlKey);
  if (learn_more_url) {
    pref.Set(prefs::kPolicyLearnMoreUrlKey, *learn_more_url);
  }
  prefs->SetValue(prefs::kBruschettaInstallerConfiguration,
                  base::Value(std::move(pref)));
}

}  // namespace bruschetta
