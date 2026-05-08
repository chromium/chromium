// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_experimental_triggering_settings_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

using base::Value::Type::INTEGER;

GeminiExperimentalTriggeringSettingsPolicyHandler::
    GeminiExperimentalTriggeringSettingsPolicyHandler(
        std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
            gen_ai_default_settings_policy_handler)
    : IntRangePolicyHandler(
          key::kGeminiExperimentalTriggeringSettings,
          glic::prefs::kGlicExperimentalTriggeringPolicySettings,
          /*min=*/0,
          /*max=*/1,
          /*clamp_=*/false),
      gen_ai_default_settings_policy_handler_(
          std::move(gen_ai_default_settings_policy_handler)),
      gemini_settings_policy_handler_(
          std::make_unique<SimplePolicyHandler>(key::kGeminiSettings,
                                                prefs::kGeminiSettings,
                                                base::Value::Type::INTEGER)),
      gemini_act_on_web_settings_policy_handler_(
          std::make_unique<SimplePolicyHandler>(
              key::kGeminiActOnWebSettings,
              glic::prefs::kGlicActuationOnWeb,
              base::Value::Type::INTEGER)) {}

GeminiExperimentalTriggeringSettingsPolicyHandler::
    ~GeminiExperimentalTriggeringSettingsPolicyHandler() = default;

bool GeminiExperimentalTriggeringSettingsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* experimental_policy =
      policies.GetValue(policy_name(), INTEGER);

  if (!experimental_policy) {
    return true;
  }

  if (!IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  bool experimental_policy_enabled = experimental_policy->GetInt() == 0;

  if (experimental_policy_enabled) {
    PrefValueMap prefs;

    // Check GeminiSettings
    if (gemini_settings_policy_handler_->CheckPolicySettings(policies,
                                                             errors)) {
      gemini_settings_policy_handler_->ApplyPolicySettings(policies, &prefs);
    }
    // Check GenAiDefaultSettings
    if (gen_ai_default_settings_policy_handler_->CheckPolicySettings(policies,
                                                                     errors)) {
      gen_ai_default_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                   &prefs);
    }
    int gemini_settings_pref_value = -1;
    prefs.GetInteger(prefs::kGeminiSettings, &gemini_settings_pref_value);

    // 0 = Enabled, 1 = Disabled for these policies.
    bool gemini_disabled = gemini_settings_pref_value == 1;

    if (gemini_disabled) {
      errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                       "GeminiSettings", "Enabled");
      return false;
    }

    // Check GeminiActOnWebSettings
    if (gemini_act_on_web_settings_policy_handler_->CheckPolicySettings(
            policies, errors)) {
      gemini_act_on_web_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                      &prefs);
    }
    int act_on_web_pref_value = -1;
    prefs.GetInteger(glic::prefs::kGlicActuationOnWeb, &act_on_web_pref_value);

    bool act_on_web_disabled = act_on_web_pref_value == 1;

    if (act_on_web_disabled) {
      errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                       "GeminiActOnWebSettings", "Enabled");
      return false;
    }
  }

  return true;
}

}  // namespace policy
