// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_controller.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_bubble_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

DictationBubbleController::DictationBubbleController() {
  ui::InputMethod* input_method =
      Shell::Get()->window_tree_host_manager()->input_method();
  if (!input_method_observer_.IsObservingSource(input_method))
    input_method_observer_.Observe(input_method);
}

DictationBubbleController::~DictationBubbleController() {
  input_method_observer_.Reset();
  if (widget_ && !widget_->IsClosed())
    widget_->CloseNow();
}

void DictationBubbleController::UpdateBubble(
    bool visible,
    DictationBubbleIconType icon,
    const std::optional<std::u16string>& text,
    const std::optional<std::vector<DictationBubbleHintType>>& hints) {
  MaybeInitialize();
  Update(icon, text, hints);
  visible ? widget_->Show() : widget_->Hide();

  for (Observer& observer : observers_) {
    observer.OnBubbleUpdated();
  }
}

void DictationBubbleController::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE ||
      !dictation_bubble_view_ || !dictation_bubble_view_->GetVisible()) {
    return;
  }

  const gfx::Rect new_caret_bounds = client->GetCaretBounds();
  if (new_caret_bounds == dictation_bubble_view_->GetAnchorRect())
    return;

  // Update the position of `dictation_bubble_view_` to match the current caret
  // location.
  dictation_bubble_view_->SetAnchorRect(new_caret_bounds);
}

void DictationBubbleController::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view != dictation_bubble_view_)
    return;
  dictation_bubble_view_->views::View::RemoveObserver(this);
  dictation_bubble_view_ = nullptr;
  widget_ = nullptr;
}

void DictationBubbleController::MaybeInitialize() {
  if (widget_)
    return;

  dictation_bubble_view_ = new DictationBubbleView();
  dictation_bubble_view_->views::View::AddObserver(this);

  widget_ =
      views::BubbleDialogDelegateView::CreateBubble(dictation_bubble_view_);
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kDictationBubble);
}

void DictationBubbleController::Update(
    DictationBubbleIconType icon,
    const std::optional<std::u16string>& text,
    const std::optional<std::vector<DictationBubbleHintType>>& hints) {
  DCHECK(dictation_bubble_view_);
  DCHECK(widget_);

  // Update `dictation_bubble_view_`.
  dictation_bubble_view_->Update(icon, text, hints);

  // Update the bounds to fit entirely within the screen.
  gfx::Rect new_bounds = widget_->GetWindowBoundsInScreen();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(new_bounds).bounds();
  new_bounds.AdjustToFit(display_bounds);

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::AvoidObstacles(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget_->GetNativeWindow()),
      new_bounds, CollisionDetectionUtils::RelativePriority::kDictationBubble);
  widget_->SetBounds(resting_bounds);
}

void DictationBubbleController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DictationBubbleController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
