// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_highlight_controller.h"

#include <vector>

#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/shell.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr char kHighlightCallerId[] = "HighlightController";
// The color for the keyboard focus ring. (The same orange color as ChromeVox.)
const SkColor kFocusColor = SkColorSetRGB(247, 152, 58);

// Returns the input method shared between ash and the browser for in-process
// ash. Returns null for out-of-process ash.
ui::InputMethod* GetSharedInputMethod() {
  return Shell::Get()->window_tree_host_manager()->input_method();
}

void SetFocusRing(AccessibilityFocusRingController* controller,
                  std::vector<gfx::Rect> rects) {
  auto focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  focus_ring->rects_in_screen = rects;
  focus_ring->behavior = FocusRingBehavior::FADE_OUT;
  focus_ring->type = FocusRingType::GLOW;
  focus_ring->color = kFocusColor;

  controller->SetFocusRing(kHighlightCallerId, std::move(focus_ring));
}

}  // namespace

AccessibilityHighlightController::AccessibilityHighlightController() {
  Shell::Get()->AddPreTargetHandler(this);
  Shell::Get()->cursor_manager()->AddObserver(this);

  GetSharedInputMethod()->AddObserver(this);
}

AccessibilityHighlightController::~AccessibilityHighlightController() {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  SetFocusRing(controller, std::vector<gfx::Rect>());
  controller->HideCaretRing();
  controller->HideCursorRing();

  GetSharedInputMethod()->RemoveObserver(this);
  Shell::Get()->cursor_manager()->RemoveObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
}

void AccessibilityHighlightController::HighlightFocus(bool focus) {
  focus_ = focus;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightController::HighlightCursor(bool cursor) {
  cursor_ = cursor;
  UpdateCursorHighlight();
}

void AccessibilityHighlightController::HighlightCaret(bool caret) {
  caret_ = caret;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightController::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {
  focus_rect_ = bounds_in_screen;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightController::SetCaretBounds(
    const gfx::Rect& caret_bounds_in_screen) {
  gfx::Point new_caret_point = caret_bounds_in_screen.CenterPoint();
  ::wm::ConvertPointFromScreen(Shell::GetPrimaryRootWindow(), &new_caret_point);
  bool new_caret_visible = IsCaretVisible(caret_bounds_in_screen);
  if (new_caret_point == caret_point_ && new_caret_visible == caret_visible_)
    return;
  caret_point_ = new_caret_point;
  caret_visible_ = IsCaretVisible(caret_bounds_in_screen);
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMouseMoved ||
      event->type() == ui::EventType::kMouseDragged) {
    cursor_point_ = event->location();
    if (event->target()) {
      ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                                 &cursor_point_);
    }
    UpdateCursorHighlight();
  }
}

void AccessibilityHighlightController::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::EventType::kKeyPressed) {
    UpdateFocusAndCaretHighlights();
  }
}

void AccessibilityHighlightController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    caret_visible_ = false;
    UpdateFocusAndCaretHighlights();
  }
}

void AccessibilityHighlightController::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    caret_visible_ = false;
    return;
  }
  SetCaretBounds(client->GetCaretBounds());
}

void AccessibilityHighlightController::OnCursorVisibilityChanged(
    bool is_visible) {
  UpdateCursorHighlight();
}

bool AccessibilityHighlightController::IsCursorVisible() {
  return Shell::Get()->cursor_manager()->IsCursorVisible();
}

bool AccessibilityHighlightController::IsCaretVisible(
    const gfx::Rect& caret_bounds_in_screen) {
  // Empty bounds are not visible. Don't use IsEmpty() because web contents
  // carets can have positive height but zero width.
  if (caret_bounds_in_screen.width() == 0 &&
      caret_bounds_in_screen.height() == 0) {
    return false;
  }

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* active_window =
      ::wm::GetActivationClient(root_window)->GetActiveWindow();
  if (!active_window)
    active_window = root_window;
  return active_window->GetBoundsInScreen().Contains(caret_point_);
}

void AccessibilityHighlightController::UpdateFocusAndCaretHighlights() {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();

  // The caret highlight takes precedence over the focus highlight if
  // both are visible.
  if (caret_ && caret_visible_) {
    controller->SetCaretRing(caret_point_);
    SetFocusRing(controller, std::vector<gfx::Rect>());
  } else if (focus_) {
    controller->HideCaretRing();
    std::vector<gfx::Rect> rects;
    if (!focus_rect_.IsEmpty())
      rects.push_back(focus_rect_);
    SetFocusRing(controller, rects);
  } else {
    controller->HideCaretRing();
    SetFocusRing(controller, std::vector<gfx::Rect>());
  }
}

void AccessibilityHighlightController::UpdateCursorHighlight() {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  if (cursor_ && IsCursorVisible())
    controller->SetCursorRing(cursor_point_);
  else
    controller->HideCursorRing();
}

}  // namespace ash
