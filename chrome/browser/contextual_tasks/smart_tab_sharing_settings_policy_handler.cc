// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/smart_tab_sharing_settings_policy_handler.h"

#include "base/logging.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

using base::Value::Type::INTEGER;

SmartTabSharingSettingsPolicyHandler::SmartTabSharingSettingsPolicyHandler(
    std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
        gen_ai_default_settings_policy_handler)
    : IntRangePolicyHandler(
          key::kSmartTabSharingSettings,
          contextual_tasks::kContextualTasksSmartTabSharingSettings,
          /*min=*/0,
          /*max=*/1,
          /*clamp_=*/false),
      gen_ai_default_settings_policy_handler_(
          std::move(gen_ai_default_settings_policy_handler)),
      search_content_sharing_settings_policy_handler_(
          std::make_unique<SimplePolicyHandler>(
              key::kSearchContentSharingSettings,
              contextual_search::kSearchContentSharingSettings,
              base::Value::Type::INTEGER)) {}

SmartTabSharingSettingsPolicyHandler::~SmartTabSharingSettingsPolicyHandler() =
    default;

bool SmartTabSharingSettingsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* smart_tab_sharing_settings_policy =
      policies.GetValue(policy_name(), INTEGER);

  if (!smart_tab_sharing_settings_policy) {
    return true;
  }

  if (!IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // See `SmartTabSharingSettings.yaml`.
  bool smart_tab_sharing_settings_policy_enabled =
      smart_tab_sharing_settings_policy->GetInt() == 0;

  if (smart_tab_sharing_settings_policy_enabled) {
    PrefValueMap prefs;
    // Check the policy value for kSearchContentSharingSettings first, then
    // fallback to kGenAiDefaultSettings.
    if (search_content_sharing_settings_policy_handler_->CheckPolicySettings(
            policies, errors)) {
      search_content_sharing_settings_policy_handler_->ApplyPolicySettings(
          policies, &prefs);
    }
    if (gen_ai_default_settings_policy_handler_->CheckPolicySettings(policies,
                                                                     errors)) {
      gen_ai_default_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                   &prefs);
    }
    int search_content_sharing_settings_pref_value = -1;
    prefs.GetInteger(contextual_search::kSearchContentSharingSettings,
                     &search_content_sharing_settings_pref_value);
    // See `SearchContentSharingSettings.yaml`.
    bool search_content_sharing_settings_disabled =
        search_content_sharing_settings_pref_value == 1;
    if (search_content_sharing_settings_disabled) {
      errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                       key::kSearchContentSharingSettings, "Enabled");
      return false;
    }
  }

  return true;
}

}  // namespace policy
