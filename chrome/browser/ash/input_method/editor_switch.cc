// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_switch.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_identity_utils.h"
#include "chrome/browser/ash/input_method/url_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/common/locale_util.h"
#include "components/manta/manta_service.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

constexpr std::string_view kCountryAllowlist[] = {
    "au", "be", "ca", "ch", "cz", "de", "dk", "es", "fi",
    "fr", "gb", "ie", "in", "it", "jp", "lu", "mx", "no",
    "nz", "nl", "pl", "pt", "se", "us", "za",
};

constexpr ui::TextInputType kTextInputTypeAllowlist[] = {
    ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE, ui::TEXT_INPUT_TYPE_TEXT,
    ui::TEXT_INPUT_TYPE_TEXT_AREA};

constexpr AppType kAppTypeDenylist[] = {
    AppType::ARC_APP,
    AppType::CROSTINI_APP,
};

const char* kWorkspaceDomainsWithPathDenylist[][2] = {
    {"calendar.google", ""}, {"docs.google", ""},      {"drive.google", ""},
    {"keep.google", ""},     {"mail.google", "/chat"}, {"mail.google", "/mail"},
    {"meet.google", ""},
};

const char* kWorkspaceAppIdDenylist[] = {
    extension_misc::kGmailAppId,
    extension_misc::kCalendarAppId,
    extension_misc::kGoogleDocsAppId,
    extension_misc::kGoogleSlidesAppId,
    extension_misc::kGoogleSheetsAppId,
    extension_misc::kGoogleDriveAppId,
    extension_misc::kGoogleKeepAppId,
    extension_misc::kGoogleMeetPwaAppId,
    extension_misc::kGoogleDocsPwaAppId,
    extension_misc::kGoogleSheetsPwaAppId,
    // App ids in demo mode
    extension_misc::kCalendarDemoAppId,
    extension_misc::kGoogleDocsDemoAppId,
    extension_misc::kGoogleSheetsDemoAppId,
    extension_misc::kGoogleSlidesDemoAppId,
    web_app::kGmailAppId,
    web_app::kGoogleChatAppId,
    web_app::kGoogleMeetAppId,
    web_app::kGoogleDocsAppId,
    web_app::kGoogleSlidesAppId,
    web_app::kGoogleSheetsAppId,
    web_app::kGoogleDriveAppId,
    web_app::kGoogleKeepAppId,
    web_app::kGoogleCalendarAppId,
};

const char* kNonWorkspaceAppIdDenylist[] = {
    extension_misc::kFilesManagerAppId,
    file_manager::kFileManagerSwaAppId,
};

constexpr int kTextLengthMaxLimit = 10000;

constexpr char kExperimentName[] = "OrcaEnabled";

constexpr char kImeAllowlistLabel[] = "ime_allowlist";

manta::FeatureSupportStatus FetchOrcaAccountCapabilityFromMantaService(
    Profile* profile) {
  if (manta::MantaService* service =
          manta::MantaServiceFactory::GetForProfile(profile)) {
    return service->SupportsOrca();
  }

  return manta::FeatureSupportStatus::kUnknown;
}

bool IsProfileManaged(Profile* profile) {
  policy::ProfilePolicyConnector* profile_policy_connector =
      profile->GetProfilePolicyConnector();

  return (profile_policy_connector != nullptr &&
          profile_policy_connector->IsManaged());
}

bool IsGoogleInternalAccountEmailFromProfile(Profile* profile) {
  std::optional<std::string> user_email =
      GetSignedInUserEmailFromProfile(profile);

  return user_email.has_value() &&
         gaia::IsGoogleInternalAccountEmail(*user_email);
}

bool IsCountryAllowed(std::string_view country_code) {
  return base::Contains(kCountryAllowlist, country_code);
}

bool IsInputTypeAllowed(ui::TextInputType type) {
  return base::Contains(kTextInputTypeAllowlist, type);
}

bool IsInputMethodEngineAllowed(const std::vector<std::string>& allowlist,
                                std::string_view engine_id) {
  for (auto& ime : allowlist) {
    if (engine_id == ime) {
      return true;
    }
  }
  return false;
}

bool IsAppTypeAllowed(AppType app_type) {
  return !base::Contains(kAppTypeDenylist, app_type);
}

bool IsTriggerableFromConsentStatus(ConsentStatus consent_status) {
  return consent_status == ConsentStatus::kApproved ||
         consent_status == ConsentStatus::kPending ||
         consent_status == ConsentStatus::kUnset;
}

bool IsUrlAllowed(Profile* profile, GURL url) {
  if (IsGoogleInternalAccountEmailFromProfile(profile) &&
      base::FeatureList::IsEnabled(features::kOrcaOnWorkspace)) {
    return true;
  }

  for (auto& denied_domain_with_path : kWorkspaceDomainsWithPathDenylist) {
    if (IsSubDomainWithPathPrefix(url, denied_domain_with_path[0],
                                  denied_domain_with_path[1])) {
      return false;
    }
  }
  return true;
}

bool IsAppAllowed(Profile* profile, std::string_view app_id) {
  if (base::Contains(kNonWorkspaceAppIdDenylist, app_id)) {
    return false;
  }

  return (IsGoogleInternalAccountEmailFromProfile(profile) &&
          base::FeatureList::IsEnabled(features::kOrcaOnWorkspace)) ||
         !base::Contains(kWorkspaceAppIdDenylist, app_id);
}

bool IsTriggerableFromTextLength(int text_length) {
  return text_length <= kTextLengthMaxLimit;
}

std::vector<std::string> GetAllowedInputMethodEngines() {
  // Default English IMEs.
  std::vector<std::string> allowed_imes = {
      "xkb:ca:eng:eng",           // Canada
      "xkb:gb::eng",              // UK
      "xkb:gb:extd:eng",          // UK Extended
      "xkb:gb:dvorak:eng",        // UK Dvorak
      "xkb:in::eng",              // India
      "xkb:pk::eng",              // Pakistan
      "xkb:us:altgr-intl:eng",    // US Extended
      "xkb:us:colemak:eng",       // US Colemak
      "xkb:us:dvorak:eng",        // US Dvorak
      "xkb:us:dvp:eng",           // US Programmer Dvorak
      "xkb:us:intl_pc:eng",       // US Intl (PC)
      "xkb:us:intl:eng",          // US Intl
      "xkb:us:workman-intl:eng",  // US Workman Intl
      "xkb:us:workman:eng",       // US Workman
      "xkb:us::eng",              // US,
      "xkb:za:gb:eng"             // South Africa
  };

  // Loads allowed imes from field trials
  if (auto parsed = base::JSONReader::Read(
          base::GetFieldTrialParamValue(kExperimentName, kImeAllowlistLabel));
      parsed.has_value() && parsed->is_list()) {
    for (const auto& item : parsed->GetList()) {
      if (item.is_string()) {
        allowed_imes.push_back(item.GetString());
      }
    }
  }

  return allowed_imes;
}

}  // namespace

bool IsAllowedForUseInDemoMode(std::string_view country_code) {
  return base::FeatureList::IsEnabled(chromeos::features::kOrca) &&
         base::FeatureList::IsEnabled(
             chromeos::features::kFeatureManagementOrca) &&
         IsCountryAllowed(country_code);
}

bool IsAllowedForUseInNonDemoMode(Profile* profile,
                                  std::string_view country_code) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kOrca) ||
      !base::FeatureList::IsEnabled(
          chromeos::features::kFeatureManagementOrca) ||
      !IsCountryAllowed(country_code) ||
      (base::FeatureList::IsEnabled(
           ash::features::kOrcaUseAccountCapabilities) &&
       FetchOrcaAccountCapabilityFromMantaService(profile) !=
           manta::FeatureSupportStatus::kSupported)) {
    return false;
  }

  // Always allow the feature on unmanaged users.
  if (!IsProfileManaged(profile)) {
    return true;
  }

  // For managed users, if the feature flag `OrcaControlledByPolicy `is set, let
  // the feature enablement be driven by the policy.
  if (base::FeatureList::IsEnabled(features::kOrcaControlledByPolicy)) {
    return profile->GetPrefs()->IsManagedPreference(prefs::kManagedOrcaEnabled)
               ? profile->GetPrefs()->GetBoolean(prefs::kManagedOrcaEnabled)
               : false;
  }

  // If the Orca policy is not ready to launch on managed users, disallow the
  // feature.
  return false;
}

bool IsSystemInEnglishLanguage() {
  return g_browser_process != nullptr &&
         language::ExtractBaseLanguage(
             g_browser_process->GetApplicationLocale()) == "en";
}

EditorSwitch::EditorSwitch(Delegate* delegate,
                           Profile* profile,
                           std::string_view country_code)
    : delegate_(delegate),
      profile_(profile),
      country_code_(country_code),
      ime_allowlist_(GetAllowedInputMethodEngines()) {}

EditorSwitch::~EditorSwitch() = default;

bool EditorSwitch::IsAllowedForUse() const {
  if (base::FeatureList::IsEnabled(chromeos::features::kOrcaDogfood)) {
    return true;
  }

  if (profile_ == nullptr) {
    return false;
  }

  if (chromeos::IsKioskSession()) {
    return false;
  }

  return base::FeatureList::IsEnabled(ash::features::kOrcaSupportDemoMode) &&
                 ash::DemoSession::IsDeviceInDemoMode()
             ? IsAllowedForUseInDemoMode(country_code_)
             : IsAllowedForUseInNonDemoMode(profile_, country_code_);
}

EditorOpportunityMode EditorSwitch::GetEditorOpportunityMode() const {
  if (IsAllowedForUse() && IsInputTypeAllowed(input_type_)) {
    return text_length_ > 0 ? EditorOpportunityMode::kRewrite
                            : EditorOpportunityMode::kWrite;
  }
  return EditorOpportunityMode::kNone;
}

std::vector<EditorBlockedReason> EditorSwitch::GetBlockedReasons() const {
  std::vector<EditorBlockedReason> blocked_reasons;

  if (base::FeatureList::IsEnabled(chromeos::features::kOrca)) {
    if (!IsCountryAllowed(country_code_)) {
      blocked_reasons.push_back(
          EditorBlockedReason::kBlockedByUnsupportedRegion);
    }

    if (IsProfileManaged(profile_)) {
      blocked_reasons.push_back(EditorBlockedReason::kBlockedByManagedStatus);
    }

    if (base::FeatureList::IsEnabled(
            ash::features::kOrcaUseAccountCapabilities)) {
      switch (FetchOrcaAccountCapabilityFromMantaService(profile_)) {
        case manta::FeatureSupportStatus::kUnsupported:
          blocked_reasons.push_back(
              EditorBlockedReason::kBlockedByUnsupportedCapability);
          break;
        case manta::FeatureSupportStatus::kUnknown:
          blocked_reasons.push_back(
              EditorBlockedReason::kBlockedByUnknownCapability);
          break;
        case manta::FeatureSupportStatus::kSupported:
          break;
      }
    }
  }

  if (!IsTriggerableFromConsentStatus(GetConsentStatusFromInteger(
          profile_->GetPrefs()->GetInteger(prefs::kOrcaConsentStatus)))) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByConsent);
  }

  if (!profile_->GetPrefs()->GetBoolean(prefs::kOrcaEnabled)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedBySetting);
  }

  if (!IsTriggerableFromTextLength(text_length_)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByTextLength);
  }

  if (!IsUrlAllowed(profile_, url_)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByUrl);
  }

  if (!IsAppAllowed(profile_, app_id_)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByApp);
  }

  if (!IsAppTypeAllowed(app_type_)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByAppType);
  }

  if (!IsInputMethodEngineAllowed(ime_allowlist_, active_engine_id_)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByInputMethod);
  }

  if (!IsInputTypeAllowed(input_type_)) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByInputType);
  }

  if (tablet_mode_enabled_) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByInvalidFormFactor);
  }

  if (net::NetworkChangeNotifier::IsOffline()) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByNetworkStatus);
  }

  return blocked_reasons;
}

bool EditorSwitch::CanBeTriggered() const {
  if (profile_ == nullptr) {
    return false;
  }

  ConsentStatus current_consent_status = GetConsentStatusFromInteger(
      profile_->GetPrefs()->GetInteger(prefs::kOrcaConsentStatus));

  return IsAllowedForUse() &&
         IsInputMethodEngineAllowed(ime_allowlist_, active_engine_id_) &&
         IsInputTypeAllowed(input_type_) && IsAppTypeAllowed(app_type_) &&
         IsTriggerableFromConsentStatus(current_consent_status) &&
         IsUrlAllowed(profile_, url_) && IsAppAllowed(profile_, app_id_) &&
         !net::NetworkChangeNotifier::IsOffline() && !tablet_mode_enabled_ &&
         // user pref value
         profile_->GetPrefs()->GetBoolean(prefs::kOrcaEnabled) &&
         text_length_ <= kTextLengthMaxLimit &&
         (!base::FeatureList::IsEnabled(features::kOrcaOnlyInEnglishLocales) ||
          IsSystemInEnglishLanguage());
}

EditorMode EditorSwitch::GetEditorMode() const {
  if (!CanBeTriggered()) {
    return EditorMode::kBlocked;
  }

  ConsentStatus current_consent_status = GetConsentStatusFromInteger(
      profile_->GetPrefs()->GetInteger(prefs::kOrcaConsentStatus));

  if (current_consent_status == ConsentStatus::kPending ||
      current_consent_status == ConsentStatus::kUnset) {
    return EditorMode::kConsentNeeded;
  } else if (text_length_ > 0) {
    return EditorMode::kRewrite;
  } else {
    return EditorMode::kWrite;
  }
}

void EditorSwitch::OnInputContextUpdated(
    const TextInputMethod::InputContext& input_context,
    const TextFieldContextualInfo& text_field_contextual_info) {
  EditorMode prev_mode = GetEditorMode();
  input_type_ = input_context.type;
  app_type_ = text_field_contextual_info.app_type;
  url_ = text_field_contextual_info.tab_url;
  app_id_ = text_field_contextual_info.app_key;
  MaybeNotifyEditorModeChanged(prev_mode);
}

void EditorSwitch::OnActivateIme(std::string_view engine_id) {
  EditorMode prev_mode = GetEditorMode();
  active_engine_id_ = engine_id;
  MaybeNotifyEditorModeChanged(prev_mode);
}

void EditorSwitch::OnTabletModeUpdated(bool is_enabled) {
  EditorMode prev_mode = GetEditorMode();
  tablet_mode_enabled_ = is_enabled;
  MaybeNotifyEditorModeChanged(prev_mode);
}

void EditorSwitch::OnTextSelectionLengthChanged(size_t text_length) {
  EditorMode prev_mode = GetEditorMode();
  text_length_ = text_length;
  MaybeNotifyEditorModeChanged(prev_mode);
}

void EditorSwitch::SetProfile(Profile* profile) {
  profile_ = profile;
}

void EditorSwitch::MaybeNotifyEditorModeChanged(const EditorMode& prev_mode) {
  EditorMode new_mode = GetEditorMode();
  if (prev_mode != new_mode) {
    delegate_->OnEditorModeChanged(new_mode);
  }
}

}  // namespace ash::input_method
