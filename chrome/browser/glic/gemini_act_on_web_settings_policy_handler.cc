// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_act_on_web_settings_policy_handler.h"

#include "base/logging.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

using base::Value::Type::INTEGER;

GeminiActOnWebSettingsPolicyHandler::GeminiActOnWebSettingsPolicyHandler(
    std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
        gemini_settings_policy_handler)
    : IntRangePolicyHandler(key::kGeminiActOnWebSettings,
                            glic::prefs::kGlicActuationOnWeb,
                            /*min=*/0,
                            /*max=*/1,
                            /*clamp_=*/false),
      gen_ai_default_settings_policy_handler_(
          std::move(gemini_settings_policy_handler)),
      gemini_settings_policy_handler_(
          std::make_unique<SimplePolicyHandler>(key::kGeminiSettings,
                                                prefs::kGeminiSettings,
                                                base::Value::Type::INTEGER)) {}

GeminiActOnWebSettingsPolicyHandler::~GeminiActOnWebSettingsPolicyHandler() =
    default;

bool GeminiActOnWebSettingsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* act_on_web_policy =
      policies.GetValue(policy_name(), INTEGER);

  if (!act_on_web_policy) {
    return true;
  }

  if (!IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // See `GeminiActOnWebSettings.yaml`.
  bool act_on_web_policy_enabled = act_on_web_policy->GetInt() == 0;

  if (act_on_web_policy_enabled) {
    PrefValueMap prefs;
    // Check the policy value for kGeminiSettings first, then fallback to
    // kGenAiDefaultSettings.
    if (gemini_settings_policy_handler_->CheckPolicySettings(policies,
                                                             errors)) {
      gemini_settings_policy_handler_->ApplyPolicySettings(policies, &prefs);
    }
    if (gen_ai_default_settings_policy_handler_->CheckPolicySettings(policies,
                                                                     errors)) {
      gen_ai_default_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                   &prefs);
    }
    int gemini_settings_pref_value = -1;
    prefs.GetInteger(prefs::kGeminiSettings, &gemini_settings_pref_value);
    // See `GeminiSettings.yaml`.
    bool gemini_disabled = gemini_settings_pref_value == 1;
    if (gemini_disabled) {
       // TODO(crbug.com/448383819): Add an approved error message to `errors`.
      LOG(ERROR) << "GeminiActOnWebSettings policy is enabled, but Gemini is "
                    "disabled.";
      return false;
    }
  }

  return true;
}

}  // namespace policy
