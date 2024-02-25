// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_STUB_INPUT_METHOD_ENGINE_OBSERVER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_STUB_INPUT_METHOD_ENGINE_OBSERVER_H_

#include <vector>

#include "chrome/browser/ash/input_method/input_method_engine_observer.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"

namespace ash {
namespace input_method {

class StubInputMethodEngineObserver : public InputMethodEngineObserver {
 public:
  StubInputMethodEngineObserver() = default;
  ~StubInputMethodEngineObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnFocus(const std::string& engine_id,
               int context_id,
               const TextInputMethod::InputContext& context) override {}
  void OnBlur(const std::string& engine_id, int context_id) override {}
  void OnKeyEvent(const std::string& engine_id,
                  const ui::KeyEvent& event,
                  TextInputMethod::KeyEventDoneCallback callback) override {}
  void OnCandidateClicked(const std::string& engine_id,
                          int candidate_id,
                          MouseButtonEvent button) override {}
  void OnAssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::u16string& text,
                                const gfx::Range selection_range,
                                int offset) override {}
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnReset(const std::string& engine_id) override {}
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}
  void OnInputMethodOptionsChanged(const std::string& engine_id) override {}
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_STUB_INPUT_METHOD_ENGINE_OBSERVER_H_
