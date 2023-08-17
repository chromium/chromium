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

bool IsInputTypeAllowed(ui::TextInputType type) {
  return base::Contains(kTextInputTypeAllowlist, type);
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
    const TextInputMethod::InputContext& input_context) {
  input_type_ = input_context.type;
  UpdateTriggerableCache();
}

void EditorSwitch::UpdateTriggerableCache() {
  can_be_triggered_ = is_allowed_for_use_ && IsInputTypeAllowed(input_type_);
}

}  // namespace ash::input_method
