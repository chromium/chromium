// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {
namespace {
// UI specs.
constexpr SkColor kEditModeBgColor = SkColorSetA(SK_ColorBLACK, 0x66 /*40%*/);

// Return true if `v1` is on top than `v2`, or `v1` is on the left side of `v2`
// when `v1` has the same y position as `v2`.
bool CompareActionViewPosition(const ActionView* v1, const ActionView* v2) {
  const auto center1 = v1->GetTouchCenterInWindow();
  const auto center2 = v2->GetTouchCenterInWindow();

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
    if (auto view = action->CreateView(controller_)) {
      AddChildView(std::move(view));
    }
  }

  controller_->AddTouchInjectorObserver(this);
}

InputMappingView::~InputMappingView() {
  controller_->RemoveTouchInjectorObserver(this);
}

void InputMappingView::SetDisplayMode(const DisplayMode mode) {
  if (current_display_mode_ == mode ||
      (mode != DisplayMode::kEdit && mode != DisplayMode::kView)) {
    return;
  }

  if (mode == DisplayMode::kView) {
    SetBackground(nullptr);
  } else {
    SortChildren();
    SetBackground(views::CreateSolidBackground(kEditModeBgColor));
  }

  for (views::View* view : children()) {
    if (auto* action_view = views::AsViewClass<ActionView>(view)) {
      action_view->SetDisplayMode(mode);
    }
  }
  current_display_mode_ = mode;
}

void InputMappingView::ProcessPressedEvent(const ui::LocatedEvent& event) {
  auto event_location = event.root_location();
  for (views::View* const child : children()) {
    if (auto* action_view = views::AsViewClass<ActionView>(child)) {
      for (arc::input_overlay::ActionLabel* action_label :
           action_view->labels()) {
        if (!action_label->HasFocus()) {
          continue;
        }
        if (auto bounds = action_label->GetBoundsInScreen();
            !bounds.Contains(event_location)) {
          action_label->ClearFocus();
          controller_->AddEditMessage(
              l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS),
              MessageType::kInfo);
          break;
        }
      }
    }
  }
}

void InputMappingView::SortChildren() {
  std::vector<ActionView*> left, right;
  const float aspect_ratio = (float)width() / height();
  for (views::View* child : children()) {
    if (auto* action_view = views::AsViewClass<ActionView>(child)) {
      if (aspect_ratio > 1.0f &&
          action_view->GetTouchCenterInWindow().x() < width() / 2) {
        left.emplace_back(action_view);
      } else {
        right.emplace_back(action_view);
      }
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

void InputMappingView::OnActionAddedInternal(Action& action) {
  // No add function for pre-beta version.
  DCHECK(IsBeta());

  if (auto view = action.CreateView(controller_)) {
    AddChildView(std::move(view))->SetDisplayMode(current_display_mode_);
  }
}

void InputMappingView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed) {
    ProcessPressedEvent(*event);
  }
}

void InputMappingView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap ||
      event->type() == ui::EventType::kGestureTapDown) {
    ProcessPressedEvent(*event);
  }
}

void InputMappingView::OnActionAdded(Action& action) {
  // No add function for pre-beta version.
  DCHECK(IsBeta());

  OnActionAddedInternal(action);
  // A new button options menu corresponding to the action is
  // added when the action is newly added.
  controller_->AddButtonOptionsMenuWidget(&action);
}

void InputMappingView::OnActionRemoved(const Action& action) {
  // No remove function for pre-beta version.
  DCHECK(IsBeta());

  for (views::View* const child : children()) {
    if (auto* action_view = views::AsViewClass<ActionView>(child);
        action_view->action() == &action) {
      RemoveChildViewT(action_view);
      break;
    }
  }
}

void InputMappingView::OnActionNewStateRemoved(const Action& action) {
  for (views::View* const child : children()) {
    if (auto* action_view = views::AsViewClass<ActionView>(child);
        action_view->action() == &action) {
      action_view->RemoveNewState();
      break;
    }
  }
}

void InputMappingView::OnActionTypeChanged(Action* action, Action* new_action) {
  // No action type change function for pre-beta version.
  DCHECK(IsBeta());
  OnActionRemoved(*action);
  OnActionAddedInternal(*new_action);
}

void InputMappingView::OnActionInputBindingUpdated(const Action& action) {
  // Action is updated in another function already for pre-beta version.
  if (!IsBeta()) {
    return;
  }

  for (views::View* const child : children()) {
    if (auto* action_view = views::AsViewClass<ActionView>(child);
        action_view->action() == &action) {
      action_view->OnActionInputBindingUpdated();
      break;
    }
  }
}

void InputMappingView::OnContentBoundsSizeChanged() {
  if (!IsBeta()) {
    return;
  }

  for (views::View* const child : children()) {
    if (auto* child_view = views::AsViewClass<ActionView>(child)) {
      child_view->OnContentBoundsSizeChanged();
    }
  }
}

BEGIN_METADATA(InputMappingView)
END_METADATA

}  // namespace arc::input_overlay
