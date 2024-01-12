// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view_test_api.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_model.h"

namespace {

// A class used to wait for animations.
class TestAPIAnimationObserver : public views::BoundsAnimatorObserver {
 public:
  explicit TestAPIAnimationObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  TestAPIAnimationObserver(const TestAPIAnimationObserver&) = delete;
  TestAPIAnimationObserver& operator=(const TestAPIAnimationObserver&) = delete;

  ~TestAPIAnimationObserver() override = default;

  // views::BoundsAnimatorObserver overrides:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

namespace ash {

ShelfViewTestAPI::ShelfViewTestAPI(ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

ShelfViewTestAPI::~ShelfViewTestAPI() = default;

size_t ShelfViewTestAPI::GetButtonCount() {
  return shelf_view_->view_model_->view_size();
}

ShelfAppButton* ShelfViewTestAPI::GetButton(int index) {
  return static_cast<ShelfAppButton*>(GetViewAt(index));
}

ShelfID ShelfViewTestAPI::AddItem(ShelfItemType type) {
  ShelfItem item;
  item.type = type;
  item.id = ShelfID(base::NumberToString(id_++));
  shelf_view_->model_->Add(item,
                           std::make_unique<TestShelfItemDelegate>(item.id));
  return item.id;
}

views::View* ShelfViewTestAPI::GetViewAt(int index) {
  return shelf_view_->view_model_->view_at(index);
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
  base::RunLoop loop;
  if (!bounds_animator->IsAnimating())
    return;

  std::unique_ptr<TestAPIAnimationObserver> observer(
      new TestAPIAnimationObserver(loop.QuitWhenIdleClosure()));

  bounds_animator->AddObserver(observer.get());

  // This nested loop will quit when TestAPIAnimationObserver's
  // OnBoundsAnimatorDone is called.
  loop.Run();

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

const gfx::Rect& ShelfViewTestAPI::visible_shelf_item_bounds_union() const {
  return shelf_view_->visible_shelf_item_bounds_union_;
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

ShelfButtonPressedMetricTracker*
ShelfViewTestAPI::shelf_button_pressed_metric_tracker() {
  return &(shelf_view_->shelf_button_pressed_metric_tracker_);
}

void ShelfViewTestAPI::SetShelfContextMenuCallback(
    base::RepeatingClosure closure) {
  DCHECK(shelf_view_->context_menu_shown_callback_.is_null());
  shelf_view_->context_menu_shown_callback_ = std::move(closure);
}

std::optional<size_t> ShelfViewTestAPI::GetSeparatorIndex() const {
  return shelf_view_->separator_index_;
}

bool ShelfViewTestAPI::IsSeparatorVisible() const {
  return shelf_view_->separator_->GetVisible();
}

bool ShelfViewTestAPI::HasPendingPromiseAppRemoval(
    const std::string& promise_app_id) const {
  auto found = shelf_view_->pending_promise_apps_removals_.find(promise_app_id);

  return found != shelf_view_->pending_promise_apps_removals_.end();
}

}  // namespace ash
