// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"

namespace arc::input_overlay {
namespace {
// UI specs.
constexpr SkColor kEditModeBgColor = SkColorSetA(SK_ColorBLACK, 0x66 /*40%*/);

// Return true if `v1` is on top than `v2`, or `v1` is on the left side of `v2`
// when `v1` has the same y position as `v2`.
bool CompareActionViewPosition(const ActionView* v1, const ActionView* v2) {
  auto center1 = v1->GetTouchCenterInWindow();
  auto center2 = v2->GetTouchCenterInWindow();

  if (center1.y() != center2.y()) {
    return center1.y() < center2.y();
  }
  return center1.x() < center2.x();
}

}  // namespace

InputMappingView::InputMappingView(
    DisplayOverlayController* display_overlay_controller)
    : controller_(display_overlay_controller) {
  SetSize(controller_->touch_injector()->content_bounds().size());
  for (auto& action : controller_->touch_injector()->actions()) {
    if (action->IsDeleted()) {
      continue;
    }
    auto view = action->CreateView(controller_);
    if (view) {
      AddChildView(std::move(view));
    }
  }

  controller_->AddTouchInjectorObserver(this);
}

InputMappingView::~InputMappingView() {
  controller_->RemoveTouchInjectorObserver(this);
}

void InputMappingView::SetDisplayMode(const DisplayMode mode) {
  DCHECK(mode != DisplayMode::kEducation);
  if (current_display_mode_ == mode || mode == DisplayMode::kMenu ||
      mode == DisplayMode::kPreMenu) {
    return;
  }
  switch (mode) {
    case DisplayMode::kView:
      SetBackground(nullptr);
      break;
    case DisplayMode::kEdit:
      SortChildren();
      SetBackground(views::CreateSolidBackground(kEditModeBgColor));
      break;
    default:
      NOTREACHED();
      break;
  }

  for (auto* view : children()) {
    auto* action_view = static_cast<ActionView*>(view);
    action_view->SetDisplayMode(mode);
  }
  current_display_mode_ = mode;
}

void InputMappingView::ProcessPressedEvent(const ui::LocatedEvent& event) {
  auto event_location = event.root_location();
  for (auto* const child : children()) {
    auto* action_view = static_cast<ActionView*>(child);
    for (auto* action_label : action_view->labels()) {
      if (!action_label->HasFocus()) {
        continue;
      }
      auto bounds = action_label->GetBoundsInScreen();
      if (!bounds.Contains(event_location)) {
        action_label->ClearFocus();
        controller_->AddEditMessage(
            l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS),
            MessageType::kInfo);
        break;
      }
    }
  }
}

void InputMappingView::SortChildren() {
  std::vector<ActionView*> left, right;
  float aspect_ratio = (float)width() / height();
  for (auto* child : children()) {
    auto* action_view = static_cast<ActionView*>(child);
    if (aspect_ratio > 1 &&
        action_view->GetTouchCenterInWindow().x() < width() / 2) {
      left.emplace_back(action_view);
    } else {
      right.emplace_back(action_view);
    }
  }

  std::sort(left.begin(), left.end(), CompareActionViewPosition);
  std::sort(right.begin(), right.end(), CompareActionViewPosition);

  for (auto* child : left) {
    AddChildView(child);
  }
  for (auto* child : right) {
    AddChildView(child);
  }
}

void InputMappingView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    ProcessPressedEvent(*event);
  }
}

void InputMappingView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressedEvent(*event);
  }
}

void InputMappingView::OnActionAdded(Action& action) {
  // No add function for pre-beta version.
  DCHECK(IsBeta());

  auto view = action.CreateView(controller_);
  if (view) {
    AddChildView(std::move(view))->SetDisplayMode(current_display_mode_);
  }
}

void InputMappingView::OnActionRemoved(const Action& action) {
  // No remove function for pre-beta version.
  DCHECK(IsBeta());

  for (auto* const child : children()) {
    auto* action_view = static_cast<ActionView*>(child);
    if (action_view->action() == &action) {
      RemoveChildViewT(action_view);
      break;
    }
  }
}

void InputMappingView::OnActionTypeChanged(Action* action, Action* new_action) {
  // No action type change function for pre-beta version.
  DCHECK(IsBeta());
  OnActionRemoved(*action);
  OnActionAdded(*new_action);
}

void InputMappingView::OnActionInputBindingUpdated(const Action& action) {
  // Action is updated in another function already for pre-beta version.
  if (!IsBeta()) {
    return;
  }

  for (auto* const child : children()) {
    auto* action_view = static_cast<ActionView*>(child);
    if (action_view->action() == &action) {
      action_view->OnActionInputBindingUpdated();
      break;
    }
  }
}

void InputMappingView::OnContentBoundsSizeChanged() {
  if (!IsBeta()) {
    return;
  }

  for (auto* const child : children()) {
    static_cast<ActionView*>(child)->OnContentBoundsSizeChanged();
  }
}
}  // namespace arc::input_overlay
