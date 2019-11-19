// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace ui {
class IMEEngineHandlerInterface;
class KeyEvent;
}  // namespace ui

namespace chromeos {

class MockInputMethodEngine : public ui::IMEEngineHandlerInterface {
 public:
  MockInputMethodEngine();
  ~MockInputMethodEngine() override;

  // IMEEngineHandlerInterface overrides.
  void FocusIn(
      const IMEEngineHandlerInterface::InputContext& input_context) override;
  void FocusOut() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void Reset() override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback callback) override;
  void SetSurroundingText(const std::string& text,
                          uint32_t cursor_pos,
                          uint32_t anchor_pos,
                          uint32_t offset_pos) override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;
  void PropertyActivate(const std::string& property_name) override;
  void CandidateClicked(uint32_t index) override;
  void SetMirroringEnabled(bool mirroring_enabled) override;
  void SetCastingEnabled(bool casting_enabled) override;

  const std::string& GetActiveComponentId() const;

  std::string last_activated_property() const {
    return last_activated_property_;
  }

 private:
  std::string active_component_id_;

  std::string last_activated_property_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
