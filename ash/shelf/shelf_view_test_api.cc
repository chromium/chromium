// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view_test_api.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "base/run_loop.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_model.h"

namespace {

// A class used to wait for animations.
class TestAPIAnimationObserver : public views::BoundsAnimatorObserver {
 public:
  TestAPIAnimationObserver() = default;
  ~TestAPIAnimationObserver() override = default;

  // views::BoundsAnimatorObserver overrides:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAPIAnimationObserver);
};

}  // namespace

namespace ash {

ShelfViewTestAPI::ShelfViewTestAPI(ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

ShelfViewTestAPI::~ShelfViewTestAPI() = default;

int ShelfViewTestAPI::GetButtonCount() {
  return shelf_view_->view_model_->view_size();
}

ShelfAppButton* ShelfViewTestAPI::GetButton(int index) {
  return static_cast<ShelfAppButton*>(GetViewAt(index));
}

ShelfID ShelfViewTestAPI::AddItem(ShelfItemType type) {
  ShelfItem item;
  item.type = type;
  item.id = ShelfID(base::NumberToString(id_++));
  shelf_view_->model_->Add(item);
  return item.id;
}

views::View* ShelfViewTestAPI::GetViewAt(int index) {
  return shelf_view_->view_model_->view_at(index);
}

void ShelfViewTestAPI::ShowOverflowBubble() {
  DCHECK(!shelf_view_->IsShowingOverflowBubble());
  shelf_view_->ToggleOverflowBubble();
}

void ShelfViewTestAPI::HideOverflowBubble() {
  DCHECK(shelf_view_->IsShowingOverflowBubble());
  shelf_view_->ToggleOverflowBubble();
}

const gfx::Rect& ShelfViewTestAPI::GetBoundsByIndex(int index) {
  return shelf_view_->view_model_->view_at(index)->bounds();
}

const gfx::Rect& ShelfViewTestAPI::GetIdealBoundsByIndex(int index) {
  return shelf_view_->view_model_->ideal_bounds(index);
}

base::TimeDelta ShelfViewTestAPI::GetAnimationDuration() const {
  DCHECK(shelf_view_->bounds_animator_);
  return shelf_view_->bounds_animator_->GetAnimationDuration();
}

void ShelfViewTestAPI::SetAnimationDuration(base::TimeDelta duration) {
  shelf_view_->bounds_animator_->SetAnimationDuration(duration);
}

void ShelfViewTestAPI::RunMessageLoopUntilAnimationsDone(
    views::BoundsAnimator* bounds_animator) {
  std::unique_ptr<TestAPIAnimationObserver> observer(
      new TestAPIAnimationObserver());

  bounds_animator->AddObserver(observer.get());

  // This nested loop will quit when TestAPIAnimationObserver's
  // OnBoundsAnimatorDone is called.
  base::RunLoop().Run();

  bounds_animator->RemoveObserver(observer.get());
}

void ShelfViewTestAPI::RunMessageLoopUntilAnimationsDone() {
  if (!shelf_view_->bounds_animator_->IsAnimating())
    return;

  RunMessageLoopUntilAnimationsDone(shelf_view_->bounds_animator_.get());
}

gfx::Rect ShelfViewTestAPI::GetMenuAnchorRect(const views::View& source,
                                              const gfx::Point& location,
                                              bool context_menu) const {
  return shelf_view_->GetMenuAnchorRect(source, location, context_menu);
}

bool ShelfViewTestAPI::CloseMenu() {
  if (!shelf_view_->IsShowingMenu())
    return false;

  shelf_view_->shelf_menu_model_adapter_->Cancel();
  return true;
}

OverflowBubble* ShelfViewTestAPI::overflow_bubble() {
  return shelf_view_->overflow_bubble_.get();
}

ShelfTooltipManager* ShelfViewTestAPI::tooltip_manager() {
  return shelf_view_->shelf()->tooltip();
}

int ShelfViewTestAPI::GetMinimumDragDistance() const {
  return ShelfView::kMinimumDragDistance;
}

bool ShelfViewTestAPI::SameDragType(ShelfItemType typea,
                                    ShelfItemType typeb) const {
  return shelf_view_->SameDragType(typea, typeb);
}

gfx::Rect ShelfViewTestAPI::GetBoundsForDragInsertInScreen() {
  return shelf_view_->GetBoundsForDragInsertInScreen();
}

bool ShelfViewTestAPI::IsRippedOffFromShelf() {
  return shelf_view_->dragged_off_shelf_;
}

bool ShelfViewTestAPI::DraggedItemToAnotherShelf() {
  return shelf_view_->dragged_to_another_shelf_;
}

ShelfButtonPressedMetricTracker*
ShelfViewTestAPI::shelf_button_pressed_metric_tracker() {
  return &(shelf_view_->shelf_button_pressed_metric_tracker_);
}

}  // namespace ash
