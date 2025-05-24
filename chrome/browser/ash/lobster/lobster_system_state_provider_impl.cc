// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider_impl.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/generative_ai_country_restrictions.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "base/containers/fixed_flat_set.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace {

ash::LobsterConsentStatus GetConsentStatusFromInteger(int status_value) {
  switch (status_value) {
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kUnset):
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kPending):
      return ash::LobsterConsentStatus::kUnset;
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kApproved):
      return ash::LobsterConsentStatus::kApproved;
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kDeclined):
      return ash::LobsterConsentStatus::kDeclined;
    default:
      LOG(ERROR) << "Invalid consent status: " << status_value;
      // For any of the invalid states, treat the consent status as unset.
      return ash::LobsterConsentStatus::kUnset;
  }
}

std::string GetCurrentImeEngineId() {
  ash::input_method::InputMethodManager* input_method_manager =
      ash::input_method::InputMethodManager::Get();
  if (input_method_manager == nullptr ||
      input_method_manager->GetActiveIMEState() == nullptr) {
    return "";
  }
  return ash::extension_ime_util::GetComponentIDByInputMethodID(
      input_method_manager->GetActiveIMEState()->GetCurrentInputMethod().id());
}

bool IsInputTypeAllowed(ui::TextInputType type) {
  static constexpr auto kTextInputTypeAllowlist =
      base::MakeFixedFlatSet<ui::TextInputType>(
          {ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE, ui::TEXT_INPUT_TYPE_TEXT,
           ui::TEXT_INPUT_TYPE_TEXT_AREA});

  return kTextInputTypeAllowlist.contains(type);
}

bool IsImeAllowed(std::string_view current_ime_id) {
  static constexpr auto kImeAllowlist =
      base::MakeFixedFlatSet<std::string_view>({
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
          "xkb:us::eng",              // US
          "xkb:za:gb:eng"             // South Africa
      });

  return kImeAllowlist.contains(current_ime_id);
}

specialized_features::FeatureAccessConfig CreateFeatureAccessConfig() {
  specialized_features::FeatureAccessConfig config;
  config.disabled_in_kiosk_mode = true;

  // Dogfood devices ignore all other checks.
  if (base::FeatureList::IsEnabled(ash::features::kLobsterDogfood)) {
    return config;
  }

  config.feature_flag = &ash::features::kLobster;
  config.feature_management_flag = &ash::features::kFeatureManagementLobster;
  config.capability_callback =
      base::BindRepeating([](AccountCapabilities capabilities) {
        return capabilities.can_use_manta_service();
      });
  config.country_codes = ash::GetGenerativeAiCountryAllowlist();
  return config;
}

}  // namespace

LobsterSystemStateProviderImpl::LobsterSystemStateProviderImpl(
    PrefService* pref,
    signin::IdentityManager* identity_manager,
    bool is_in_demo_mode)
    : pref_(pref),
      access_checker_(CreateFeatureAccessConfig(),
                      pref_,
                      identity_manager,
                      /*variations_service_callback=*/base::BindRepeating([]() {
                        return g_browser_process->variations_service();
                      })),
      is_in_demo_mode_(is_in_demo_mode) {}

LobsterSystemStateProviderImpl::~LobsterSystemStateProviderImpl() = default;

ash::LobsterSystemState LobsterSystemStateProviderImpl::GetSystemState(
    const ash::LobsterTextInputContext& text_input_context) {
  ash::LobsterSystemState system_state(ash::LobsterStatus::kEnabled,
                                       /*failed_checks=*/{});

  specialized_features::FeatureAccessFailureSet access_checker_failure_set =
      access_checker_.Check();

  // Performs feature flag check
  if (access_checker_failure_set.Has(
          specialized_features::FeatureAccessFailure::kFeatureFlagDisabled)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kInvalidFeatureFlags);
  }

  // Performs a hardware check
  if (access_checker_failure_set.Has(
          specialized_features::FeatureAccessFailure::
              kFeatureManagementCheckFailed)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kUnsupportedHardware);
  }

  // Performs a location check
  if (access_checker_failure_set.Has(
          specialized_features::FeatureAccessFailure::kCountryCheckFailed)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kInvalidRegion);
  }

  // TODO: b:406915099 - Migrate demo mode check into the shared feature checker module.
  // Performs account capabilities check in non-demo mode only
  if (!is_in_demo_mode_ && access_checker_failure_set.Has(
                               specialized_features::FeatureAccessFailure::
                                   kAccountCapabilitiesCheckFailed)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kInvalidAccountCapabilities);
  }

  // Performs a kiosk mode check
  if (access_checker_failure_set.Has(
          specialized_features::FeatureAccessFailure::
              kDisabledInKioskModeCheckFailed)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kUnsupportedInKioskMode);
  }

  if (!IsInputTypeAllowed(text_input_context.text_input_type)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kInvalidInputField);
  }

  if (!ash::features::IsLobsterEnabledForManagedUsers() &&
      pref_->IsManagedPreference(
          ash::prefs::kLobsterEnterprisePolicySettings)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kForcedDisabledOnManagedUsers);
  }

  if (pref_->GetInteger(ash::prefs::kLobsterEnterprisePolicySettings) ==
      base::to_underlying(ash::LobsterEnterprisePolicyValue::kDisabled)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kUnsupportedPolicy);
  }

  if (!pref_->GetBoolean(ash::prefs::kLobsterEnabled)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kSettingsOff);
  }

  // Performs a network check
  if (net::NetworkChangeNotifier::IsOffline()) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kNoInternetConnection);
  }

  // Performs a tablet mode check
  if (is_in_tablet_mode_) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kUnsupportedFormFactor);
  }

  // Performs an IME check
  if (ash::features::IsLobsterDisabledByInvalidIME() &&
      !IsImeAllowed(GetCurrentImeEngineId())) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kInvalidInputMethod);
  }

  ash::LobsterConsentStatus consent_status = GetConsentStatusFromInteger(
      pref_->GetInteger(ash::prefs::kOrcaConsentStatus));

  if (consent_status == ash::LobsterConsentStatus::kDeclined) {
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kInvalidConsent);
  }

  if (!system_state.failed_checks.empty()) {
    system_state.status = ash::LobsterStatus::kBlocked;
  } else if (consent_status == ash::LobsterConsentStatus::kUnset) {
    system_state.status = ash::LobsterStatus::kConsentNeeded;
  } else {
    system_state.status = ash::LobsterStatus::kEnabled;
  }

  return system_state;
}

void LobsterSystemStateProviderImpl::OnDisplayTabletStateChanged(
    display::TabletState state) {
  is_in_tablet_mode_ = (state == display::TabletState::kInTabletMode);
}
