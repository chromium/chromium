// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_INPUT_METHOD_OBSERVER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_INPUT_METHOD_OBSERVER_H_

#include "base/macros.h"
#include "ui/base/ime/input_method_observer.h"

namespace ui {
class InputMethod;
}

namespace ash {

// Observes an input method for text input caret changes. Forwards the caret
// bounds to ash over mojo (via AccessibilityManager) so that ash can show the
// caret highlight ring.
class AccessibilityInputMethodObserver : public ui::InputMethodObserver {
 public:
  // |input_method| must outlive this object.
  explicit AccessibilityInputMethodObserver(ui::InputMethod* input_method);
  ~AccessibilityInputMethodObserver() override;

  // Resets the caret bounds.
  void ResetCaretBounds();

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnShowVirtualKeyboardIfEnabled() override {}

 private:
  ui::InputMethod* const input_method_;

  // Sends the caret bounds to ash.
  void UpdateCaretBounds(const ui::TextInputClient* client);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityInputMethodObserver);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_INPUT_METHOD_OBSERVER_H_
