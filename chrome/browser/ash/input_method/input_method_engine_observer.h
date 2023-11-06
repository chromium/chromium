// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_ENGINE_OBSERVER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_ENGINE_OBSERVER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/events/event.h"

namespace ui {
class TextInputMethod;
class KeyEvent;

namespace ime {
struct AssistiveWindowButton;
}  // namespace ime
}  // namespace ui

namespace ash {
namespace ime {
struct AssistiveWindow;
}  // namespace ime

namespace input_method {

enum MouseButtonEvent {
  MOUSE_BUTTON_LEFT,
  MOUSE_BUTTON_RIGHT,
  MOUSE_BUTTON_MIDDLE,
};

class InputMethodEngineObserver {
 public:
  virtual ~InputMethodEngineObserver() = default;

  // Called when the IME becomes the active IME.
  virtual void OnActivate(const std::string& engine_id) = 0;

  // Called when a text field gains focus, and will be sending key events.
  // `context_id` is a unique ID given to this focus session.
  virtual void OnFocus(const std::string& engine_id,
                       int context_id,
                       const TextInputMethod::InputContext& context) = 0;

  // Called when a text field loses focus, and will no longer generate events.
  virtual void OnBlur(const std::string& engine_id, int context_id) = 0;

  // Called when the user pressed a key with a text field focused.
  virtual void OnKeyEvent(const std::string& engine_id,
                          const ui::KeyEvent& event,
                          TextInputMethod::KeyEventDoneCallback key_data) = 0;

  // Called when Chrome terminates on-going text input session.
  virtual void OnReset(const std::string& engine_id) = 0;

  // Called when the IME is no longer active.
  virtual void OnDeactivated(const std::string& engine_id) = 0;

  // Called when the caret bounds change.
  virtual void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) = 0;

  // Called when a surrounding text is changed.
  virtual void OnSurroundingTextChanged(const std::string& engine_id,
                                        const std::u16string& text,
                                        gfx::Range selection_range,
                                        int offset_pos) = 0;

  // Called when the user clicks on an item in the candidate list.
  virtual void OnCandidateClicked(const std::string& component_id,
                                  int candidate_id,
                                  MouseButtonEvent button) = 0;

  // Called when the user clicks on a button in assistive window.
  virtual void OnAssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) {}

  // Called when there are changes to the assistive window shown to the user.
  virtual void OnAssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) = 0;

  // Called when a menu item for this IME is interacted with.
  virtual void OnMenuItemActivated(const std::string& component_id,
                                   const std::string& menu_id) = 0;

  virtual void OnScreenProjectionChanged(bool is_projected) = 0;

  // Called when the suggestions to display have changed.
  virtual void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) = 0;

  // Called when the input method options are updated.
  virtual void OnInputMethodOptionsChanged(const std::string& engine_id) = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_ENGINE_OBSERVER_H_
