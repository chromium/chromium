// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/test/apps_grid_view_test_api.h"

#include <vector>

#include "ash/app_list/paged_view_structure.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/events/event.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"

namespace ash {
namespace test {

namespace {

class BoundsAnimatorWaiter : public views::BoundsAnimatorObserver {
 public:
  explicit BoundsAnimatorWaiter(views::BoundsAnimator* animator)
      : animator_(animator) {
    animator->AddObserver(this);
  }
  ~BoundsAnimatorWaiter() override { animator_->RemoveObserver(this); }

  void Wait() {
    if (!animator_->IsAnimating())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  // views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  views::BoundsAnimator* animator_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimatorWaiter);
};

}  // namespace

AppsGridViewTestApi::AppsGridViewTestApi(AppsGridView* view) : view_(view) {}

AppsGridViewTestApi::~AppsGridViewTestApi() {}

views::View* AppsGridViewTestApi::GetViewAtModelIndex(int index) const {
  return view_->view_model_.view_at(index);
}

void AppsGridViewTestApi::LayoutToIdealBounds() {
  if (view_->reorder_timer_.IsRunning()) {
    view_->reorder_timer_.Stop();
    view_->OnReorderTimer();
  }
  if (view_->folder_dropping_timer_.IsRunning()) {
    view_->folder_dropping_timer_.Stop();
    view_->OnFolderDroppingTimer();
  }
  view_->bounds_animator_->Cancel();
  view_->Layout();
}

gfx::Rect AppsGridViewTestApi::GetItemTileRectOnCurrentPageAt(int row,
                                                              int col) const {
  int slot = row * (view_->cols()) + col;
  return view_->GetExpectedTileBounds(
      GridIndex(view_->pagination_model()->selected_page(), slot));
}

void AppsGridViewTestApi::PressItemAt(int index) {
  GetViewAtModelIndex(index)->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE));
}

bool AppsGridViewTestApi::HasPendingPageFlip() const {
  return view_->page_flip_timer_.IsRunning() ||
         view_->pagination_model()->has_transition();
}

int AppsGridViewTestApi::TilesPerPage(int page) const {
  return view_->TilesPerPage(page);
}

int AppsGridViewTestApi::AppsOnPage(int page) const {
  return view_->view_structure_.items_on_page(page);
}

AppListItemView* AppsGridViewTestApi::GetViewAtIndex(GridIndex index) const {
  return view_->GetViewAtIndex(index);
}

views::View* AppsGridViewTestApi::GetViewAtVisualIndex(int page,
                                                       int slot) const {
  const std::vector<std::vector<AppListItemView*>>& view_structure =
      view_->view_structure_.pages();
  if (page >= static_cast<int>(view_structure.size()) ||
      slot >= static_cast<int>(view_structure[page].size())) {
    return nullptr;
  }
  return view_structure[page][slot];
}

gfx::Rect AppsGridViewTestApi::GetItemTileRectAtVisualIndex(int page,
                                                            int slot) const {
  return view_->GetExpectedTileBounds(GridIndex(page, slot));
}

void AppsGridViewTestApi::WaitForItemMoveAnimationDone() {
  BoundsAnimatorWaiter waiter(view_->bounds_animator_.get());
  waiter.Wait();
}

}  // namespace test
}  // namespace ash
