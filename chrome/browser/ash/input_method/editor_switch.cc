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
#include "chrome/browser/ash/input_method/input_methods_by_language.h"
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
#include "chromeos/ui/base/window_properties.h"
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

constexpr chromeos::AppType kAppTypeDenylist[] = {
    chromeos::AppType::ARC_APP,
    chromeos::AppType::CROSTINI_APP,
};

const char* kWorkspaceDomainsWithPathDenylist[][2] = {
    {"calendar.google", ""}, {"docs.google", ""},      {"drive.google", ""},
    {"keep.google", ""},     {"mail.google", "/chat"}, {"mail.google", "/mail"},
    {"meet.google", ""},     {"script.google", ""},    {"sites.google", ""},
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

std::vector<std::string> Combine(
    const std::vector<std::vector<std::string>>& vecs) {
  std::vector<std::string> combined;
  for (auto& vec : vecs) {
    combined.insert(combined.end(), vec.begin(), vec.end());
  }
  return combined;
}

const std::vector<std::string>& AllowedInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods(
      base::FeatureList::IsEnabled(chromeos::features::kOrcaInternationalize)
          ? Combine({EnglishInputMethods(), FrenchInputMethods(),
                     GermanInputMethods(), JapaneseInputMethods()})
          : EnglishInputMethods());
  return *input_methods;
}

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

bool IsAppTypeAllowed(chromeos::AppType app_type) {
  if (base::FeatureList::IsEnabled(features::kOrcaArc) &&
      app_type == chromeos::AppType::ARC_APP) {
    return true;
  }
  return !base::Contains(kAppTypeDenylist, app_type);
}

bool IsTriggerableFromConsentStatus(ConsentStatus consent_status) {
  return consent_status == ConsentStatus::kApproved ||
         consent_status == ConsentStatus::kPending ||
         consent_status == ConsentStatus::kUnset;
}

bool IsUrlAllowed(GURL url) {
  if (base::FeatureList::IsEnabled(features::kOrcaOnWorkspace)) {
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

bool IsAppAllowed(std::string_view app_id) {
  if (base::Contains(kNonWorkspaceAppIdDenylist, app_id)) {
    return false;
  }

  return base::FeatureList::IsEnabled(features::kOrcaOnWorkspace) ||
         !base::Contains(kWorkspaceAppIdDenylist, app_id);
}

bool IsTriggerableFromTextLength(int text_length) {
  return text_length <= kTextLengthMaxLimit;
}

std::vector<std::string> GetAllowedInputMethodEngines() {
  std::vector<std::string> allowed_imes = AllowedInputMethods();

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

EditorSwitch::EditorSwitch(Observer* observer,
                           Profile* profile,
                           EditorContext* context)
    : observer_(observer),
      profile_(profile),
      context_(context),
      ime_allowlist_(GetAllowedInputMethodEngines()),
      last_known_editor_mode_(GetEditorMode()) {}

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
             ? IsAllowedForUseInDemoMode(context_->active_country_code())
             : IsAllowedForUseInNonDemoMode(profile_,
                                            context_->active_country_code());
}

EditorOpportunityMode EditorSwitch::GetEditorOpportunityMode() const {
  if (!IsAllowedForUse()) {
    return EditorOpportunityMode::kNotAllowedForUse;
  }

  if (IsInputTypeAllowed(context_->input_type())) {
    return context_->selected_text_length() > 0
               ? EditorOpportunityMode::kRewrite
               : EditorOpportunityMode::kWrite;
  }

  return EditorOpportunityMode::kInvalidInput;
}

std::vector<EditorBlockedReason> EditorSwitch::GetBlockedReasons() const {
  std::vector<EditorBlockedReason> blocked_reasons;

  if (base::FeatureList::IsEnabled(chromeos::features::kOrca)) {
    if (!IsCountryAllowed(context_->active_country_code())) {
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

  if (!IsTriggerableFromTextLength(context_->selected_text_length())) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByTextLength);
  }

  if (!IsUrlAllowed(context_->active_url())) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByUrl);
  }

  if (!IsAppAllowed(context_->app_id())) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByApp);
  }

  if (!IsAppTypeAllowed(context_->app_type())) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByAppType);
  }

  if (!IsInputMethodEngineAllowed(ime_allowlist_,
                                  context_->active_engine_id())) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByInputMethod);
  }

  if (!IsInputTypeAllowed(context_->input_type())) {
    blocked_reasons.push_back(EditorBlockedReason::kBlockedByInputType);
  }

  if (context_->InTabletMode()) {
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
         IsInputMethodEngineAllowed(ime_allowlist_,
                                    context_->active_engine_id()) &&
         IsInputTypeAllowed(context_->input_type()) &&
         IsAppTypeAllowed(context_->app_type()) &&
         IsTriggerableFromConsentStatus(current_consent_status) &&
         IsUrlAllowed(context_->active_url()) &&
         IsAppAllowed(context_->app_id()) &&
         !net::NetworkChangeNotifier::IsOffline() &&
         !context_->InTabletMode() &&
         // user pref value
         profile_->GetPrefs()->GetBoolean(prefs::kOrcaEnabled) &&
         context_->selected_text_length() <= kTextLengthMaxLimit &&
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
  } else if (context_->selected_text_length() > 0) {
    return EditorMode::kRewrite;
  } else {
    return EditorMode::kWrite;
  }
}

void EditorSwitch::OnContextUpdated() {
  EditorMode current_mode = GetEditorMode();
  if (current_mode != last_known_editor_mode_) {
    observer_->OnEditorModeChanged(current_mode);
  }
  last_known_editor_mode_ = current_mode;
}

}  // namespace ash::input_method
