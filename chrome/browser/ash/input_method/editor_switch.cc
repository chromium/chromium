// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_switch.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

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

bool IsInputTypeAllowed(ui::TextInputType type) {
  return base::Contains(kTextInputTypeAllowlist, type);
}

bool IsInputMethodEngineAllowed(std::string_view engine_id) {
  return base::Contains(kInputMethodEngineAllowlist, engine_id);
}

bool IsAppTypeAllowed(AppType app_type) {
  return base::Contains(kAppTypeAllowlist, app_type);
}

}  // namespace

EditorSwitch::EditorSwitch(bool is_managed) {
  is_allowed_for_use_ =
      (base::FeatureList::IsEnabled(features::kOrcaDogfood) && is_managed) ||
      (chromeos::features::IsOrcaEnabled() && !is_managed);
}

EditorSwitch::~EditorSwitch() = default;

bool EditorSwitch::IsAllowedForUse() {
  return is_allowed_for_use_;
}

bool EditorSwitch::CanBeTriggered() {
  return can_be_triggered_;
}

void EditorSwitch::OnInputContextUpdated(
    const TextInputMethod::InputContext& input_context,
    const TextFieldContextualInfo& text_field_contextual_info) {
  input_type_ = input_context.type;
  app_type_ = text_field_contextual_info.app_type;
  UpdateTriggerableCache();
}

void EditorSwitch::OnActivateIme(std::string_view engine_id) {
  active_engine_id_ = engine_id;
  UpdateTriggerableCache();
}

void EditorSwitch::UpdateTriggerableCache() {
  can_be_triggered_ =
      is_allowed_for_use_ && IsInputMethodEngineAllowed(active_engine_id_) &&
      IsInputTypeAllowed(input_type_) && IsAppTypeAllowed(app_type_);
}

}  // namespace ash::input_method
