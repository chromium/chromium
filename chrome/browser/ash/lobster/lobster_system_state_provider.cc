// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

#include <array>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "base/containers/fixed_flat_set.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "components/prefs/pref_service.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"

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

}  // namespace

LobsterSystemStateProvider::LobsterSystemStateProvider(Profile* profile)
    : profile_(profile) {}

LobsterSystemStateProvider::~LobsterSystemStateProvider() = default;

ash::LobsterSystemState LobsterSystemStateProvider::GetSystemState(
    const ash::LobsterTextInputContext& text_input_context) {
  ash::LobsterSystemState system_state(ash::LobsterStatus::kEnabled,
                                       /*failed_checks=*/{});

  if (!IsInputTypeAllowed(text_input_context.text_input_type)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kInvalidInputField);
  }

  if (!profile_->GetPrefs()->GetBoolean(ash::prefs::kLobsterEnabled)) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(ash::LobsterSystemCheck::kSettingsOff);
  }

  // Performs a network check
  if (net::NetworkChangeNotifier::IsOffline()) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kNoInternetConnection);
  }

  // Performs an IME check
  if (!IsImeAllowed(GetCurrentImeEngineId())) {
    system_state.status = ash::LobsterStatus::kBlocked;
    system_state.failed_checks.Put(
        ash::LobsterSystemCheck::kInvalidInputMethod);
  }

  ash::LobsterConsentStatus consent_status = GetConsentStatusFromInteger(
      profile_->GetPrefs()->GetInteger(ash::prefs::kOrcaConsentStatus));

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
