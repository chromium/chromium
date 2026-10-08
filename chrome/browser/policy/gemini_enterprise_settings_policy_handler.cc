// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/gemini_enterprise_settings_policy_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/policy/gemini_enterprise_url_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

constexpr char kUrlField[] = "url";

}  // namespace

GeminiEnterpriseSettingsPolicyHandler::GeminiEnterpriseSettingsPolicyHandler(
    const Schema& chrome_schema)
    : SimpleSchemaValidatingPolicyHandler(
          key::kGeminiEnterpriseSettings,
          glic::prefs::kGlicGeminiEnterpriseSettings,
          chrome_schema,
          SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

GeminiEnterpriseSettingsPolicyHandler::
    ~GeminiEnterpriseSettingsPolicyHandler() = default;

bool GeminiEnterpriseSettingsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                errors)) {
    return false;
  }

  // The base class accepts an unset policy and rejects any value that is not
  // an object, so a null value here means the policy is not set.
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);
  if (!value) {
    return true;
  }

  // An absent or empty `url` leaves nothing to validate.
  const std::string* url = value->GetDict().FindString(kUrlField);
  return !url || url->empty() ||
         GeminiEnterpriseUrlPolicyHandler::CheckUrl(policy_name(), *url, errors,
                                                    PolicyErrorPath{kUrlField});
}

}  // namespace policy
