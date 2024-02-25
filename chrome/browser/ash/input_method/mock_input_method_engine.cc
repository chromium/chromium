// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/mock_input_method_engine.h"

#include <map>

namespace ash {
namespace input_method {

MockInputMethodEngine::MockInputMethodEngine() = default;

MockInputMethodEngine::~MockInputMethodEngine() = default;

void MockInputMethodEngine::Focus(
    const TextInputMethod::InputContext& input_context) {}

void MockInputMethodEngine::Blur() {}

void MockInputMethodEngine::Enable(const std::string& component_id) {
  active_component_id_ = component_id;
}

void MockInputMethodEngine::Disable() {
  active_component_id_.clear();
}

void MockInputMethodEngine::Reset() {}

void MockInputMethodEngine::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                            KeyEventDoneCallback callback) {}

void MockInputMethodEngine::SetSurroundingText(const std::u16string& text,
                                               const gfx::Range selection_range,
                                               uint32_t offset_pos) {}

void MockInputMethodEngine::SetCaretBounds(const gfx::Rect& caret_bounds) {}

ui::VirtualKeyboardController*
MockInputMethodEngine::GetVirtualKeyboardController() const {
  return nullptr;
}

void MockInputMethodEngine::PropertyActivate(const std::string& property_name) {
  last_activated_property_ = property_name;
}

void MockInputMethodEngine::CandidateClicked(uint32_t index) {}

void MockInputMethodEngine::AssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) {}

void MockInputMethodEngine::SetMirroringEnabled(bool mirroring_enabled) {}

void MockInputMethodEngine::SetCastingEnabled(bool casting_enabled) {}

bool MockInputMethodEngine::IsReadyForTesting() {
  return true;
}

const std::string& MockInputMethodEngine::GetActiveComponentId() const {
  return active_component_id_;
}

}  // namespace input_method
}  // namespace ash
