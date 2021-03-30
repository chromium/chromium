// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_STUB_INPUT_METHOD_ENGINE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_STUB_INPUT_METHOD_ENGINE_OBSERVER_H_

#include <vector>

#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"

namespace chromeos {
namespace input_method {

class StubInputMethodEngineObserver : public InputMethodEngineBase::Observer {
 public:
  StubInputMethodEngineObserver() = default;
  ~StubInputMethodEngineObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnFocus(
      int context_id,
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnBlur(int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const ui::KeyEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override {}
  void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset) override {}
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnReset(const std::string& engine_id) override {}
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}
  void OnInputMethodOptionsChanged(const std::string& engine_id) override {}
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_STUB_INPUT_METHOD_ENGINE_OBSERVER_H_
