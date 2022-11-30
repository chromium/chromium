// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/text_input_method.h"

namespace ui {
class TextInputMethod;
class KeyEvent;
}  // namespace ui

namespace ash {
namespace input_method {

class MockInputMethodEngine : public ui::TextInputMethod {
 public:
  MockInputMethodEngine();
  ~MockInputMethodEngine() override;

  // TextInputMethod overrides.
  void FocusIn(const TextInputMethod::InputContext& input_context) override;
  void OnTouch(ui::EventPointerType pointerType) override;
  void FocusOut() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void Reset() override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback callback) override;
  void SetSurroundingText(const std::u16string& text,
                          uint32_t cursor_pos,
                          uint32_t anchor_pos,
                          uint32_t offset_pos) override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetCaretBounds(const gfx::Rect& caret_bounds) override;
  ui::VirtualKeyboardController* GetVirtualKeyboardController() const override;
  void PropertyActivate(const std::string& property_name) override;
  void CandidateClicked(uint32_t index) override;
  void SetMirroringEnabled(bool mirroring_enabled) override;
  void SetCastingEnabled(bool casting_enabled) override;
  bool IsReadyForTesting() override;

  const std::string& GetActiveComponentId() const;

  std::string last_activated_property() const {
    return last_activated_property_;
  }

 private:
  std::string active_component_id_;

  std::string last_activated_property_;
};

}  // namespace input_method
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
namespace input_method {
using ::ash::input_method::MockInputMethodEngine;
}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
