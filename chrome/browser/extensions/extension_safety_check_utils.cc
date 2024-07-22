// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_safety_check_utils.h"

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {

// Update the old kPrefAcknowledgeSafetyCheckWarning pref to the new
// kPrefAcknowledgeSafetyCheckWarningReason pref. If only the boolean
// acknowledged pref is present, it's replaced with the new acknowledge
// reason pref set to the current top warning reason. If both the
// boolean and reason acknowledged pref are present, the bool pref is
// removed.
void MigrateSafetyCheckAcknowledgePref(
    const Extension& extension,
    developer::SafetyCheckWarningReason acknowledged_reason,
    developer::SafetyCheckWarningReason top_warning_reason,
    ExtensionPrefs* extension_prefs) {
  bool extension_kept = false;
  extension_prefs->ReadPrefAsBoolean(
      extension.id(), extensions::kPrefAcknowledgeSafetyCheckWarning,
      &extension_kept);
  if (!extension_kept) {
    return;
  }
  if (acknowledged_reason == developer::SafetyCheckWarningReason::kNone) {
    extension_prefs->SetIntegerPref(
        extension.id(), extensions::kPrefAcknowledgeSafetyCheckWarningReason,
        static_cast<int>(top_warning_reason));
  }
  // Remove the old boolean pref.
  extension_prefs->UpdateExtensionPref(
      extension.id(), extensions::kPrefAcknowledgeSafetyCheckWarning.name,
      std::nullopt);
}

// Returns true if the Safety Check should display a malware warning.
bool SafetyCheckShouldShowMalware(
    BitMapBlocklistState blocklist_state,
    const std::optional<CWSInfoService::CWSInfo>& cws_info) {
  bool valid_cws_info = cws_info.has_value() && cws_info->is_present;
  bool has_safe_browsing_malware_rating =
      blocklist_state == BitMapBlocklistState::BLOCKLISTED_MALWARE;
  bool has_cws_malware_rating =
      valid_cws_info &&
      cws_info->violation_type == CWSInfoService::CWSViolationType::kMalware;
  bool is_malware = has_safe_browsing_malware_rating || has_cws_malware_rating;
  return is_malware;
}

// Returns true if the Safety Check should display a policy violation warning.
bool SafetyCheckShouldShowPolicyViolation(
    BitMapBlocklistState blocklist_state,
    const std::optional<CWSInfoService::CWSInfo>& cws_info) {
  bool valid_cws_info = cws_info.has_value() && cws_info->is_present;
  bool has_safe_browsing_policy_rating =
      blocklist_state == BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION;
  bool has_cws_policy_rating =
      valid_cws_info &&
      cws_info->violation_type == CWSInfoService::CWSViolationType::kPolicy;
  bool is_policy_violation =
      has_safe_browsing_policy_rating || has_cws_policy_rating;
  return is_policy_violation;
}

// Returns true if the Safety Check should display an unwanted software warning.
bool SafetyCheckShouldShowPotentiallyUnwanted(
    BitMapBlocklistState blocklist_state) {
  bool is_potentially_unwanted =
      blocklist_state == BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED;
  bool potentially_unwanted_enabled =
      base::FeatureList::IsEnabled(features::kSafetyHubExtensionsUwSTrigger);
  return potentially_unwanted_enabled && is_potentially_unwanted;
}

// Returns true if the Safety Check should display a no privacy practice
// warning.
bool SafetyCheckShouldShowNoPrivacyPractice(
    const std::optional<CWSInfoService::CWSInfo>& cws_info) {
  bool valid_cws_info = cws_info.has_value() && cws_info->is_present;
  bool no_privacy_practice_enabled = base::FeatureList::IsEnabled(
      features::kSafetyHubExtensionsNoPrivacyPracticesTrigger);
  bool has_no_privacy_practice_info =
      valid_cws_info && cws_info->no_privacy_practice;
  return no_privacy_practice_enabled && has_no_privacy_practice_info;
}

// Returns true if the Safety Check should display a no off-store extension
// warning.
bool SafetyCheckShouldShowOffstoreExtension(
    const Extension& extension,
    Profile* profile,
    const std::optional<CWSInfoService::CWSInfo>& cws_info) {
  if (!base::FeatureList::IsEnabled(
          features::kSafetyHubExtensionsOffStoreTrigger)) {
    return false;
  }
  // Leave command-line extensions out for now since removing them is only
  // effective till the next browser session.
  if (extension.location() == mojom::ManifestLocation::kCommandLine) {
    return false;
  }
  // Calculate if the extension triggers an off-store extension warning such as
  // extensions that are no longer on the Chrome Web Store.
  if (Manifest::IsUnpackedLocation(extension.location())) {
    // Extensions that are unpacked will only trigger a review if dev
    // mode is not enabled.
    bool dev_mode =
        profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
    return !dev_mode;
  } else {
    ExtensionManagement* extension_management =
        ExtensionManagementFactory::GetForBrowserContext(profile);
    bool updates_from_webstore =
        extension_management->UpdatesFromWebstore(extension);
    if (updates_from_webstore) {
      if (cws_info.has_value() && !cws_info->is_present) {
        // If the extension has a webstore update URL but is not present
        // in the webstore itself, then we will not consider it from
        // the webstore.
        return true;
      }
    } else {
      // Extension does not update from the webstore.
      return true;
    }
  }

  return false;
}

// Return the `PrefAcknowledgeSafetyCheckWarningReason` pref as an enum.
developer::SafetyCheckWarningReason GetPrefAcknowledgeSafetyCheckWarningReason(
    const Extension& extension,
    ExtensionPrefs* extension_prefs) {
  int kept_reason_int = 0;
  extension_prefs->ReadPrefAsInteger(
      extension.id(), extensions::kPrefAcknowledgeSafetyCheckWarningReason,
      &kept_reason_int);
  developer::SafetyCheckWarningReason acknowledged_reason =
      static_cast<developer::SafetyCheckWarningReason>(kept_reason_int);
  return (acknowledged_reason);
}

// Converts the `SafetyCheckWarningReason` enum into its corresponding
// warning level. The greater the return value the more severe the
// trigger.
int GetSafetyCheckWarningLevel(
    developer::SafetyCheckWarningReason safety_check_warning) {
  switch (safety_check_warning) {
    case developer::SafetyCheckWarningReason::kMalware:
      return 6;
    case developer::SafetyCheckWarningReason::kPolicy:
      return 5;
    case developer::SafetyCheckWarningReason::kUnwanted:
      return 4;
    case developer::SafetyCheckWarningReason::kUnpublished:
      return 3;
    case developer::SafetyCheckWarningReason::kNoPrivacyPractice:
      return 2;
    case developer::SafetyCheckWarningReason::kOffstore:
      return 1;
    case developer::SafetyCheckWarningReason::kNone:
      return 0;
  }
}

// Checks if the user has already acknowledged a safety check warning that
// is of the same or greater level than the current warning reason.
//    * current_warning - Current Safety Check warning reason
//    * acknowledged_warning - Previously acknowledged warning reason
bool SafetyCheckHasUserAcknowledgedWarningLevel(
    developer::SafetyCheckWarningReason acknowledged_reason,
    developer::SafetyCheckWarningReason warning_reason) {
  int acknowledged_reason_level =
      GetSafetyCheckWarningLevel(acknowledged_reason);
  int warning_reason_level = GetSafetyCheckWarningLevel(warning_reason);
  return acknowledged_reason_level >= warning_reason_level;
}

}  // namespace

namespace ExtensionSafetyCheckUtils {

developer::SafetyCheckWarningReason GetSafetyCheckWarningReason(
    const Extension& extension,
    Profile* profile,
    bool unpublished_only) {
  CWSInfoService* cws_info_service =
      CWSInfoService::Get(Profile::FromBrowserContext(profile));
  BitMapBlocklistState blocklist_state =
      blocklist_prefs::GetExtensionBlocklistState(extension.id(),
                                                  ExtensionPrefs::Get(profile));
  return GetSafetyCheckWarningReasonHelper(
      cws_info_service, blocklist_state, profile, extension, unpublished_only);
}

developer::SafetyCheckWarningReason GetSafetyCheckWarningReasonHelper(
    CWSInfoServiceInterface* cws_info_service,
    BitMapBlocklistState blocklist_state,
    Profile* profile,
    const Extension& extension,
    bool unpublished_only) {
  developer::SafetyCheckWarningReason top_warning_reason =
      developer::SafetyCheckWarningReason::kNone;
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile);
  bool is_extension = extension.is_extension() || extension.is_shared_module();
  bool is_non_visible_extension =
      extensions::Manifest::IsComponentLocation(extension.location());
  bool is_explicitly_allowed_by_policy =
      extension_management->IsInstallationExplicitlyAllowed(extension.id());
  // We do not show warnings for the following:
  // - Chrome apps
  // - Chrome extensions that are enterprise policy controlled OR not visible
  // to the user.
  if (!is_extension || is_non_visible_extension ||
      is_explicitly_allowed_by_policy) {
    return developer::SafetyCheckWarningReason::kNone;
  }

  developer::SafetyCheckWarningReason acknowledged_reason =
      GetPrefAcknowledgeSafetyCheckWarningReason(extension,
                                                 ExtensionPrefs::Get(profile));
  std::optional<CWSInfoService::CWSInfo> cws_info;
  bool valid_cws_info = false;
  if (base::FeatureList::IsEnabled(kCWSInfoService)) {
    cws_info = cws_info_service->GetCWSInfo(extension);
    valid_cws_info = cws_info.has_value() && cws_info->is_present;
  }
  if (unpublished_only) {
    if (valid_cws_info && cws_info->unpublished_long_ago) {
      top_warning_reason = developer::SafetyCheckWarningReason::kUnpublished;
    }
  } else {
    if (SafetyCheckShouldShowMalware(blocklist_state, cws_info)) {
      top_warning_reason = developer::SafetyCheckWarningReason::kMalware;
    } else if (SafetyCheckShouldShowPolicyViolation(blocklist_state,
                                                    cws_info)) {
      top_warning_reason = developer::SafetyCheckWarningReason::kPolicy;
    } else if (SafetyCheckShouldShowPotentiallyUnwanted(blocklist_state)) {
      top_warning_reason = developer::SafetyCheckWarningReason::kUnwanted;
    } else if (valid_cws_info && cws_info->unpublished_long_ago) {
      top_warning_reason = developer::SafetyCheckWarningReason::kUnpublished;

    } else if (SafetyCheckShouldShowNoPrivacyPractice(cws_info)) {
      top_warning_reason =
          developer::SafetyCheckWarningReason::kNoPrivacyPractice;

    } else if (SafetyCheckShouldShowOffstoreExtension(extension, profile,
                                                      cws_info)) {
      top_warning_reason = developer::SafetyCheckWarningReason::kOffstore;
    }
  }

  // TODO(crbug.com/325469212) Remove after migration is deemed complete.
  MigrateSafetyCheckAcknowledgePref(extension, acknowledged_reason,
                                    top_warning_reason,
                                    ExtensionPrefs::Get(profile));

  // If user has chosen to keep the extension for the current, or a higher
  // trigger reason, we will return no trigger.
  if (SafetyCheckHasUserAcknowledgedWarningLevel(acknowledged_reason,
                                                 top_warning_reason)) {
    return developer::SafetyCheckWarningReason::kNone;
  }
  return top_warning_reason;
}

api::developer_private::SafetyCheckStrings GetSafetyCheckWarningStrings(
    developer::SafetyCheckWarningReason warning_reason,
    developer::ExtensionState state) {
  developer::SafetyCheckStrings display_strings;
  int detail_string_id = -1;
  int panel_string_id = -1;
  switch (warning_reason) {
    case developer::SafetyCheckWarningReason::kMalware:
      detail_string_id = IDS_SAFETY_CHECK_EXTENSIONS_MALWARE;
      panel_string_id = IDS_EXTENSIONS_SC_MALWARE;
      break;
    case developer::SafetyCheckWarningReason::kPolicy:
      detail_string_id = IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION;
      panel_string_id = state == developer::ExtensionState::kEnabled
                            ? IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON
                            : IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF;
      break;
    case developer::SafetyCheckWarningReason::kUnwanted:
      detail_string_id = IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION;
      panel_string_id = state == developer::ExtensionState::kEnabled
                            ? IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON
                            : IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF;
      break;
    case developer::SafetyCheckWarningReason::kNoPrivacyPractice:
      detail_string_id = IDS_EXTENSIONS_SAFETY_CHECK_NO_PRIVACY_PRACTICES;
      panel_string_id =
          state == developer::ExtensionState::kEnabled
              ? IDS_EXTENSIONS_SAFETY_CHECK_NO_PRIVACY_PRACTICES_ON
              : IDS_EXTENSIONS_SAFETY_CHECK_NO_PRIVACY_PRACTICES_OFF;
      break;
    case developer::SafetyCheckWarningReason::kOffstore:
      detail_string_id = IDS_EXTENSIONS_SAFETY_CHECK_OFFSTORE;
      panel_string_id = state == developer::ExtensionState::kEnabled
                            ? IDS_EXTENSIONS_SAFETY_CHECK_OFFSTORE_ON
                            : IDS_EXTENSIONS_SAFETY_CHECK_OFFSTORE_OFF;
      break;
    case developer::SafetyCheckWarningReason::kUnpublished:
      detail_string_id = IDS_SAFETY_CHECK_EXTENSIONS_UNPUBLISHED;
      panel_string_id = state == developer::ExtensionState::kEnabled
                            ? IDS_EXTENSIONS_SC_UNPUBLISHED_ON
                            : IDS_EXTENSIONS_SC_UNPUBLISHED_OFF;
      break;
    case developer::SafetyCheckWarningReason::kNone:
      break;
  }
  if (detail_string_id != -1) {
    display_strings.detail_string = l10n_util::GetStringUTF8(detail_string_id);
    display_strings.panel_string = l10n_util::GetStringUTF8(panel_string_id);
  }
  return display_strings;
}
}  // namespace ExtensionSafetyCheckUtils
}  // namespace extensions
