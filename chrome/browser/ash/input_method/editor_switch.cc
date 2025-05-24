// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/input_method/editor_switch.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/generative_ai_country_restrictions.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/containers/extend.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/to_vector.h"
#include "base/json/json_reader.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/input_methods_by_language.h"
#include "chrome/browser/ash/input_method/url_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_enterprise_policy_enums.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_text_selection_mode.h"
#include "chromeos/ash/components/file_manager/app_id.h"
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

const char* kWorkspaceDomainsWithPathDenylist[][2] = {
    {"calendar.google", ""}, {"docs.google", ""},      {"drive.google", ""},
    {"keep.google", ""},     {"mail.google", "/chat"}, {"mail.google", "/mail"},
    {"meet.google", ""},     {"script.google", ""},    {"sites.google", ""},
};

constexpr int kTextLengthMaxLimit = 10000;

constexpr char kExperimentName[] = "OrcaEnabled";

constexpr char kImeAllowlistLabel[] = "ime_allowlist";

std::vector<std::string> AllowedInputMethods() {
  auto to_string = [](std::string_view sv) { return std::string(sv); };
  std::vector<std::string> input_methods =
      base::ToVector(EnglishInputMethods(), to_string);

  if (base::FeatureList::IsEnabled(features::kOrcaAfrikaans)) {
    base::Extend(input_methods, AfrikaansInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaDanish)) {
    base::Extend(input_methods, DanishInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaDutch)) {
    base::Extend(input_methods, DutchInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaFinnish)) {
    base::Extend(input_methods, FinnishInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaFrench)) {
    base::Extend(input_methods, FrenchInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaGerman)) {
    base::Extend(input_methods, GermanInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaItalian)) {
    base::Extend(input_methods, ItalianInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaJapanese)) {
    base::Extend(input_methods, JapaneseInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaNorwegian)) {
    base::Extend(input_methods, NorwegianInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaPolish)) {
    base::Extend(input_methods, PolishInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaPortugese)) {
    base::Extend(input_methods, PortugeseInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaSpanish)) {
    base::Extend(input_methods, SpanishInputMethods(), to_string);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaSwedish)) {
    base::Extend(input_methods, SwedishInputMethods(), to_string);
  }

  return input_methods;
}

manta::FeatureSupportStatus FetchOrcaAccountCapabilityFromMantaService(
    Profile* profile) {
  if (manta::MantaService* service =
          manta::MantaServiceFactory::GetForProfile(profile)) {
    return service->SupportsOrca();
  }

  return manta::FeatureSupportStatus::kUnknown;
}

bool IsInputTypeAllowed(ui::TextInputType type) {
  constexpr auto kTextInputTypeAllowlist =
      base::MakeFixedFlatSet<ui::TextInputType>(
          {ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE, ui::TEXT_INPUT_TYPE_TEXT,
           ui::TEXT_INPUT_TYPE_TEXT_AREA});

  return kTextInputTypeAllowlist.contains(type);
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

  constexpr auto kAppTypeDenylist = base::MakeFixedFlatSet<chromeos::AppType>({
      chromeos::AppType::ARC_APP,
      chromeos::AppType::CROSTINI_APP,
  });

  return !kAppTypeDenylist.contains(app_type);
}

bool IsTriggerableFromConsentStatus(
    chromeos::editor_menu::EditorConsentStatus consent_status) {
  return consent_status ==
             chromeos::editor_menu::EditorConsentStatus::kApproved ||
         consent_status ==
             chromeos::editor_menu::EditorConsentStatus::kPending ||
         consent_status == chromeos::editor_menu::EditorConsentStatus::kUnset;
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
  constexpr auto kNonWorkspaceAppIdDenylist =
      base::MakeFixedFlatSet<std::string_view>({
          extension_misc::kFilesManagerAppId,
          file_manager::kFileManagerSwaAppId,
      });

  if (kNonWorkspaceAppIdDenylist.contains(app_id)) {
    return false;
  }

  constexpr auto kWorkspaceAppIdDenylist =
      base::MakeFixedFlatSet<std::string_view>({
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
          ash::kGmailAppId,
          ash::kGoogleChatAppId,
          ash::kGoogleMeetAppId,
          ash::kGoogleDocsAppId,
          ash::kGoogleSlidesAppId,
          ash::kGoogleSheetsAppId,
          ash::kGoogleDriveAppId,
          ash::kGoogleKeepAppId,
          ash::kGoogleCalendarAppId,
      });

  return base::FeatureList::IsEnabled(features::kOrcaOnWorkspace) ||
         !kWorkspaceAppIdDenylist.contains(app_id);
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
         IsGenerativeAiAllowedForCountry(country_code);
}

bool IsAllowedForUseInNonDemoMode(Profile* profile,
                                  std::string_view country_code) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kOrca) ||
      !base::FeatureList::IsEnabled(
          chromeos::features::kFeatureManagementOrca) ||
      !IsGenerativeAiAllowedForCountry(country_code) ||
      (base::FeatureList::IsEnabled(
           ash::features::kOrcaUseAccountCapabilities) &&
       FetchOrcaAccountCapabilityFromMantaService(profile) !=
           manta::FeatureSupportStatus::kSupported)) {
    return false;
  }

  // Allow the feature traits to be visible (at the minimum in settings) in
  // either one scenario: (1) The feature is not driven by any policy. (2) The
  // feature is driven by a policy, and we allow the policy to take effect by
  // the feature flag value.
  return !profile->GetPrefs()->IsManagedPreference(
             prefs::kHmwManagedSettings) ||
         base::FeatureList::IsEnabled(features::kOrcaForManagedUsers);
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

// TODO: b:362381487 - Rename this method as now this method no longer includes
// the check for policy value.
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

bool EditorSwitch::IsFeedbackEnabled() const {
  if (profile_ == nullptr) {
    return false;
  }

  // If managed, check the enablement value.
  return profile_->GetPrefs()->GetInteger(prefs::kHmwManagedSettings) ==
         base::to_underlying(chromeos::editor_menu::EditorEnterprisePolicy::
                                 kAllowedWithModelImprovement);
}

bool EditorSwitch::CanShowNoticeBanner() const {
  auto* pref = profile_->GetPrefs();
  // Only show the notice when:
  //  1. Editor is forced ON by the admin, and
  //  2. The consent status is currently disabled.
  return pref->IsManagedPreference(prefs::kOrcaEnabled) &&
         pref->GetBoolean(prefs::kOrcaEnabled) &&
         chromeos::editor_menu::GetConsentStatusFromInteger(
             pref->GetInteger(prefs::kOrcaConsentStatus)) ==
             chromeos::editor_menu::EditorConsentStatus::kDeclined;
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
    if (!IsGenerativeAiAllowedForCountry(context_->active_country_code())) {
      blocked_reasons.push_back(
          EditorBlockedReason::kBlockedByUnsupportedRegion);
    }

    if (profile_->GetPrefs()->IsManagedPreference(prefs::kOrcaEnabled) &&
        !profile_->GetPrefs()->GetBoolean(prefs::kOrcaEnabled)) {
      blocked_reasons.push_back(EditorBlockedReason::kBlockedByPolicy);
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

  if (!IsTriggerableFromConsentStatus(
          chromeos::editor_menu::GetConsentStatusFromInteger(
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

  chromeos::editor_menu::EditorConsentStatus current_consent_status =
      chromeos::editor_menu::GetConsentStatusFromInteger(
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
         profile_->GetPrefs()->GetInteger(prefs::kHmwManagedSettings) !=
             base::to_underlying(
                 chromeos::editor_menu::EditorEnterprisePolicy::kDisallowed) &&
         // user pref value
         profile_->GetPrefs()->GetBoolean(prefs::kOrcaEnabled) &&
         context_->selected_text_length() <= kTextLengthMaxLimit &&
         (!base::FeatureList::IsEnabled(features::kOrcaOnlyInEnglishLocales) ||
          IsSystemInEnglishLanguage());
}

chromeos::editor_menu::EditorMode EditorSwitch::GetEditorMode() const {
  if (!IsAllowedForUse()) {
    return chromeos::editor_menu::EditorMode::kHardBlocked;
  }

  if (!CanBeTriggered()) {
    return chromeos::editor_menu::EditorMode::kSoftBlocked;
  }

  chromeos::editor_menu::EditorConsentStatus current_consent_status =
      chromeos::editor_menu::GetConsentStatusFromInteger(
          profile_->GetPrefs()->GetInteger(prefs::kOrcaConsentStatus));

  if (current_consent_status ==
          chromeos::editor_menu::EditorConsentStatus::kPending ||
      current_consent_status ==
          chromeos::editor_menu::EditorConsentStatus::kUnset) {
    return chromeos::editor_menu::EditorMode::kConsentNeeded;
  } else if (context_->selected_text_length() > 0) {
    return chromeos::editor_menu::EditorMode::kRewrite;
  } else {
    return chromeos::editor_menu::EditorMode::kWrite;
  }
}

chromeos::editor_menu::EditorTextSelectionMode
EditorSwitch::GetEditorTextSelectionMode() const {
  return context_->selected_text_length() > 0
             ? chromeos::editor_menu::EditorTextSelectionMode::kHasSelection
             : chromeos::editor_menu::EditorTextSelectionMode::kNoSelection;
}

void EditorSwitch::OnContextUpdated() {
  chromeos::editor_menu::EditorMode current_mode = GetEditorMode();
  if (current_mode != last_known_editor_mode_) {
    observer_->OnEditorModeChanged(current_mode);
  }
  last_known_editor_mode_ = current_mode;
}

}  // namespace ash::input_method
