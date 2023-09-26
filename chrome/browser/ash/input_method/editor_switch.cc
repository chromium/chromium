// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_switch.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chromeos/constants/chromeos_features.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

constexpr std::string_view kCountryAllowlist[] = {"allowed_country"};

constexpr ui::TextInputType kTextInputTypeAllowlist[] = {
    ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE, ui::TEXT_INPUT_TYPE_TEXT,
    ui::TEXT_INPUT_TYPE_TEXT_AREA};

constexpr std::string_view kInputMethodEngineAllowlist[] = {
    "xkb:gb::eng",
    "xkb:gb:extd:eng",          // UK
    "xkb:gb:dvorak:eng",        // UK Extended
    "xkb:us:altgr-intl:eng",    // US Extended
    "xkb:us:colemak:eng",       // US Colemak
    "xkb:us:dvorak:eng",        // US Dvorak
    "xkb:us:dvp:eng",           // US Programmer Dvorak
    "xkb:us:intl_pc:eng",       // US Intl (PC)
    "xkb:us:intl:eng",          // US Intl
    "xkb:us:workman-intl:eng",  // US Workman Intl
    "xkb:us:workman:eng",       // US Workman
    "xkb:us::eng",              // US
};

constexpr AppType kAppTypeAllowlist[] = {
    AppType::BROWSER,
    AppType::LACROS,
};

constexpr int kTextLengthMaxLimit = 8000;

bool IsCountryAllowed(std::string_view country_code) {
  return base::Contains(kCountryAllowlist, country_code);
}

bool IsInputTypeAllowed(ui::TextInputType type) {
  return base::Contains(kTextInputTypeAllowlist, type);
}

bool IsInputMethodEngineAllowed(std::string_view engine_id) {
  return base::Contains(kInputMethodEngineAllowlist, engine_id);
}

bool IsAppTypeAllowed(AppType app_type) {
  return base::Contains(kAppTypeAllowlist, app_type);
}

bool IsTriggerableFromConsentStatus(ConsentStatus consent_status) {
  return consent_status == ConsentStatus::kApproved ||
         consent_status == ConsentStatus::kPending ||
         consent_status == ConsentStatus::kUnset;
}

}  // namespace

EditorSwitch::EditorSwitch(Profile* profile, std::string_view country_code)
    : profile_(profile), country_code_(country_code) {}

EditorSwitch::~EditorSwitch() = default;

bool EditorSwitch::IsAllowedForUse() const {
  bool is_managed = profile_->GetProfilePolicyConnector()->IsManaged();

  return  // Conditions required for dogfooding.
      (base::FeatureList::IsEnabled(chromeos::features::kOrcaDogfood)) ||
      // Conditions required for the feature to be enabled for non-dogfood
      // population.
      (base::FeatureList::IsEnabled(chromeos::features::kOrca) &&
       base::FeatureList::IsEnabled(features::kFeatureManagementOrca) &&
       !is_managed && IsCountryAllowed(country_code_));
}

bool EditorSwitch::CanBeTriggered() const {
  ConsentStatus current_consent_status = GetConsentStatusFromInteger(
      profile_->GetPrefs()->GetInteger(prefs::kOrcaConsentStatus));

  return IsAllowedForUse() && IsInputMethodEngineAllowed(active_engine_id_) &&
         IsInputTypeAllowed(input_type_) && IsAppTypeAllowed(app_type_) &&
         IsTriggerableFromConsentStatus(current_consent_status) &&
         !net::NetworkChangeNotifier::IsOffline() && !tablet_mode_enabled_ &&
         // user pref value
         profile_->GetPrefs()->GetBoolean(prefs::kOrcaEnabled) &&
         text_length_ <= kTextLengthMaxLimit;
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
  input_type_ = input_context.type;
  app_type_ = text_field_contextual_info.app_type;
}

void EditorSwitch::OnActivateIme(std::string_view engine_id) {
  active_engine_id_ = engine_id;
}

void EditorSwitch::OnTabletModeUpdated(bool is_enabled) {
  tablet_mode_enabled_ = is_enabled;
}

void EditorSwitch::OnTextSelectionLengthChanged(size_t text_length) {
  text_length_ = text_length;
}

void EditorSwitch::SetProfile(Profile* profile) {
  profile_ = profile;
}

}  // namespace ash::input_method
