// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_HIGHLIGHT_CONTROLLER_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_HIGHLIGHT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class KeyEvent;
class InputMethod;
class MouseEvent;
class TextInputClient;
}  // namespace ui

namespace ash {

// Controls visual highlights that Chrome OS can draw around the focused object,
// the cursor, and the text caret for accessibility.
class ASH_EXPORT AccessibilityHighlightController
    : public ui::EventHandler,
      public ui::InputMethodObserver,
      public aura::client::CursorClientObserver {
 public:
  AccessibilityHighlightController();

  AccessibilityHighlightController(const AccessibilityHighlightController&) =
      delete;
  AccessibilityHighlightController& operator=(
      const AccessibilityHighlightController&) = delete;

  ~AccessibilityHighlightController() override;

  void HighlightFocus(bool focus);
  void HighlightCursor(bool cursor);
  void HighlightCaret(bool caret);
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen);

  // Updates the visual highlight position for the text input caret. Removes
  // the highlight if the caret is not visible.
  void SetCaretBounds(const gfx::Rect& caret_bounds_in_screen);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;

  // aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;

 private:
  bool IsCursorVisible();
  bool IsCaretVisible(const gfx::Rect& caret_bounds_in_screen);
  void UpdateFocusAndCaretHighlights();
  void UpdateCursorHighlight();

  bool focus_ = false;
  gfx::Rect focus_rect_;

  bool cursor_ = false;
  gfx::Point cursor_point_;

  bool caret_ = false;
  bool caret_visible_ = false;
  gfx::Point caret_point_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_HIGHLIGHT_CONTROLLER_H_
