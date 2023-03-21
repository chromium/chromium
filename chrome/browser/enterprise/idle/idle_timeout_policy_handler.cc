// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_timeout_policy_handler.h"

#include <cstring>
#include <string>

#include "base/containers/span.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_idle {

namespace {

#if !BUILDFLAG(IS_ANDROID)
const char kCloseBrowsersActionName[] = "close_browsers";
const char kShowProfilePickerActionName[] = "show_profile_picker";
#endif  // !BUILDFLAG(IS_ANDROID)
const char kClearBrowsingHistoryActionName[] = "clear_browsing_history";
const char kClearDownloadHistoryActionName[] = "clear_download_history";
const char kClearCookiesAndOtherSiteDataActionName[] =
    "clear_cookies_and_other_site_data";
const char kClearCachedImagesAndFilesActionName[] =
    "clear_cached_images_and_files";
const char kClearPasswordSigninActionName[] = "clear_password_signin";
const char kClearAutofillActionName[] = "clear_autofill";
const char kClearSiteSettingsActionName[] = "clear_site_settings";
const char kClearHostedAppDataActionName[] = "clear_hosted_app_data";
const char kReloadPagesActionName[] = "reload_pages";

// If `other_policy_name` is unset, adds an error to `errors` and returns false.
bool CheckOtherPolicySet(const policy::PolicyMap& policies,
                         const std::string& this_policy_name,
                         const std::string& other_policy_name,
                         policy::PolicyErrorMap* errors) {
  if (policies.GetValueUnsafe(other_policy_name))
    return true;

  errors->AddError(this_policy_name, IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                   other_policy_name);
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
bool RequiresSyncDisabled(const std::string& name) {
  static const char* kActionsAllowedWithSync[] = {
      kCloseBrowsersActionName,
      kShowProfilePickerActionName,
      kClearDownloadHistoryActionName,
      kClearCookiesAndOtherSiteDataActionName,
      kClearCachedImagesAndFilesActionName,
      kClearSiteSettingsActionName,
      kReloadPagesActionName,
  };
  return !base::ranges::any_of(
      base::make_span(kActionsAllowedWithSync),
      [&name](const char* s) { return !std::strcmp(s, name.c_str()); });
}
#endif  // !BUILDFLAG(IS_ANDROID)

absl::optional<ActionType> NameToActionType(const std::string& name) {
#if !BUILDFLAG(IS_ANDROID)
  if (name == kCloseBrowsersActionName) {
    return ActionType::kCloseBrowsers;
  }
  if (name == kShowProfilePickerActionName) {
    return ActionType::kShowProfilePicker;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  if (name == kClearBrowsingHistoryActionName) {
    return ActionType::kClearBrowsingHistory;
  }
  if (name == kClearDownloadHistoryActionName) {
    return ActionType::kClearDownloadHistory;
  }
  if (name == kClearCookiesAndOtherSiteDataActionName) {
    return ActionType::kClearCookiesAndOtherSiteData;
  }
  if (name == kClearCachedImagesAndFilesActionName) {
    return ActionType::kClearCachedImagesAndFiles;
  }
  if (name == kClearPasswordSigninActionName) {
    return ActionType::kClearPasswordSignin;
  }
  if (name == kClearAutofillActionName) {
    return ActionType::kClearAutofill;
  }
  if (name == kClearSiteSettingsActionName) {
    return ActionType::kClearSiteSettings;
  }
  if (name == kClearHostedAppDataActionName) {
    return ActionType::kClearHostedAppData;
  }
  if (name == kReloadPagesActionName) {
    return ActionType::kReloadPages;
  }
  return absl::nullopt;
}

}  // namespace

IdleTimeoutPolicyHandler::IdleTimeoutPolicyHandler()
    : policy::IntRangePolicyHandler(policy::key::kIdleTimeout,
                                    prefs::kIdleTimeout,
                                    1,
                                    INT_MAX,
                                    true) {}

IdleTimeoutPolicyHandler::~IdleTimeoutPolicyHandler() = default;

void IdleTimeoutPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  DCHECK(value);

  // Apply a minimum of 1.
  base::TimeDelta time_delta = base::Minutes(std::max(value->GetInt(), 1));
  prefs->SetValue(prefs::kIdleTimeout, base::TimeDeltaToValue(time_delta));
}

bool IdleTimeoutPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy_name())) {
    return false;
  }

  // Check that it's an integer, and that it's >= 1.
  if (!policy::IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // If IdleTimeoutActions is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy_name(),
                           policy::key::kIdleTimeoutActions, errors)) {
    return false;
  }

  return true;
}

IdleTimeoutActionsPolicyHandler::IdleTimeoutActionsPolicyHandler(
    policy::Schema schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kIdleTimeoutActions,
          schema.GetKnownProperty(policy::key::kIdleTimeoutActions),
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

IdleTimeoutActionsPolicyHandler::~IdleTimeoutActionsPolicyHandler() = default;

void IdleTimeoutActionsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  DCHECK(policy_value);

  // Convert strings to integers (from the ActionType enum).
  base::Value::List converted_actions;
  for (const base::Value& action : policy_value->GetList()) {
    if (!action.is_string()) {
      continue;
    }
    if (absl::optional<ActionType> action_type =
            NameToActionType(action.GetString())) {
      converted_actions.Append(static_cast<int>(action_type.value()));
    }
  }
  prefs->SetValue(prefs::kIdleTimeoutActions,
                  base::Value(std::move(converted_actions)));
}

bool IdleTimeoutActionsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy_name())) {
    return false;
  }

  // Check that it's a list of strings, and that they're supported enum values.
  // Unsupported enum values are dropped, with a warning on chrome://policy.
  if (!policy::SchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                  errors)) {
    return false;
  }

  // If IdleTimeout is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy_name(), policy::key::kIdleTimeout,
                           errors)) {
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  const base::Value* sync_disabled =
      policies.GetValue(policy::key::kSyncDisabled, base::Value::Type::BOOLEAN);
  if (!sync_disabled || !sync_disabled->GetBool()) {
    // SyncDisabled is false. Check actions that require SyncDisabled=true,
    // and show a user-friendly error message if needed.
    std::vector<std::string> invalid_actions;
    const base::Value* value =
        policies.GetValue(policy_name(), base::Value::Type::LIST);
    DCHECK(value);
    for (const base::Value& action : value->GetList()) {
      if (action.is_string() && RequiresSyncDisabled(action.GetString())) {
        invalid_actions.push_back(action.GetString());
      }
    }
    if (!invalid_actions.empty()) {
      errors->AddError(
          policy_name(), IDS_POLICY_IDLE_TIMEOUT_ACTIONS_DEPENDENCY_ERROR,
          std::vector<std::string>{policy::key::kSyncDisabled, "Enabled",
                                   base::JoinString(invalid_actions, ", ")});
      return false;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return true;
}
}  // namespace enterprise_idle
