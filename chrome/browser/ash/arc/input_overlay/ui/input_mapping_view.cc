// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"

#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"

namespace arc::input_overlay {
namespace {
// UI specs.
constexpr SkColor kEditModeBgColor = SkColorSetA(SK_ColorBLACK, 0x99);
}  // namespace

InputMappingView::InputMappingView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {
  auto content_bounds =
      display_overlay_controller_->touch_injector()->content_bounds();
  auto& actions = display_overlay_controller_->touch_injector()->actions();
  SetBounds(content_bounds.x(), content_bounds.y(), content_bounds.width(),
            content_bounds.height());
  for (auto& action : actions) {
    if (action->deleted())
      continue;
    auto view = action->CreateView(display_overlay_controller_);
    if (view)
      AddChildView(std::move(view));
  }
}

InputMappingView::~InputMappingView() = default;

void InputMappingView::SetDisplayMode(const DisplayMode mode) {
  DCHECK(mode != DisplayMode::kEducation);
  if (current_display_mode_ == mode || mode == DisplayMode::kMenu ||
      mode == DisplayMode::kPreMenu) {
    return;
  }
  if (!AllowReposition()) {
    switch (mode) {
      case DisplayMode::kView:
        SetBackground(nullptr);
        break;
      case DisplayMode::kEdit:
        SetBackground(views::CreateSolidBackground(kEditModeBgColor));
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  for (auto* view : children()) {
    auto* action_view = static_cast<ActionView*>(view);
    action_view->SetDisplayMode(mode);
  }
  current_display_mode_ = mode;
}

void InputMappingView::OnActionAdded(Action* action) {
  auto view = action->CreateView(display_overlay_controller_);
  if (view) {
    view->SetDisplayMode(current_display_mode_);
    AddChildView(std::move(view));
  }
}

void InputMappingView::OnActionRemoved(Action* action) {
  for (auto* view : children()) {
    auto* action_view = static_cast<ActionView*>(view);
    if (action != action_view->action())
      continue;

    action->set_action_view(nullptr);
    RemoveChildViewT(action_view);
    break;
  }
}

void InputMappingView::ProcessPressedEvent(const ui::LocatedEvent& event) {
  auto event_location = event.root_location();
  for (auto* const child : children()) {
    auto* action_view = static_cast<ActionView*>(child);
    for (auto* action_label : action_view->labels()) {
      if (!action_label->HasFocus())
        continue;
      auto bounds = action_label->GetBoundsInScreen();
      if (!bounds.Contains(event_location)) {
        action_label->ClearFocus();
        display_overlay_controller_->AddEditMessage(
            l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS),
            MessageType::kInfo);
        break;
      }
    }
  }
}

void InputMappingView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(*event);
}

void InputMappingView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressedEvent(*event);
  }
}

}  // namespace arc::input_overlay
