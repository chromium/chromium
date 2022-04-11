// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_test_util.h"

#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

// Gets the overview grid associated with the primary root window. Returns null
// if we aren't in overview.
OverviewGrid* GetPrimaryOverviewGrid() {
  auto* overview_session = GetOverviewSession();
  return overview_session ? overview_session->GetGridWithRootWindow(
                                Shell::GetPrimaryRootWindow())
                          : nullptr;
}

// `BoundsAnimatorWaiter` observes the `BoundsAnimator` and waits for the
// template grid items animations to finish.
class BoundsAnimatorWaiter : public views::BoundsAnimatorObserver {
 public:
  explicit BoundsAnimatorWaiter(views::BoundsAnimator& animator)
      : animator_(animator) {
    animator_.AddObserver(this);
  }

  BoundsAnimatorWaiter(const BoundsAnimatorWaiter&) = delete;
  BoundsAnimatorWaiter& operator=(const BoundsAnimatorWaiter&) = delete;

  ~BoundsAnimatorWaiter() override { animator_.RemoveObserver(this); }

  void Wait() {
    if (!animator_.IsAnimating())
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

  views::BoundsAnimator& animator_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

DesksTemplatesPresenterTestApi::DesksTemplatesPresenterTestApi(
    DesksTemplatesPresenter* presenter)
    : presenter_(presenter) {
  DCHECK(presenter_);
}

DesksTemplatesPresenterTestApi::~DesksTemplatesPresenterTestApi() = default;

void DesksTemplatesPresenterTestApi::SetOnUpdateUiClosure(
    base::OnceClosure closure) {
  DCHECK(!presenter_->on_update_ui_closure_for_testing_);
  presenter_->on_update_ui_closure_for_testing_ = std::move(closure);
}

DesksTemplatesGridViewTestApi::DesksTemplatesGridViewTestApi(
    DesksTemplatesGridView* grid_view)
    : grid_view_(grid_view) {
  DCHECK(grid_view_);
}

DesksTemplatesGridViewTestApi::~DesksTemplatesGridViewTestApi() = default;

void DesksTemplatesGridViewTestApi::WaitForItemMoveAnimationDone() {
  BoundsAnimatorWaiter(grid_view_->bounds_animator_).Wait();
}

DesksTemplatesItemViewTestApi::DesksTemplatesItemViewTestApi(
    const DesksTemplatesItemView* item_view)
    : item_view_(item_view) {
  DCHECK(item_view_);
}

DesksTemplatesItemViewTestApi::~DesksTemplatesItemViewTestApi() = default;

std::vector<DesksTemplatesIconView*>
DesksTemplatesItemViewTestApi::GetIconViews() const {
  std::vector<DesksTemplatesIconView*> casted_icon_views;
  for (auto* icon_view : item_view_->icon_container_view_->children()) {
    casted_icon_views.push_back(
        static_cast<DesksTemplatesIconView*>(icon_view));
  }
  return casted_icon_views;
}

DesksTemplatesIconViewTestApi::DesksTemplatesIconViewTestApi(
    const DesksTemplatesIconView* desks_templates_icon_view)
    : desks_templates_icon_view_(desks_templates_icon_view) {
  DCHECK(desks_templates_icon_view_);
}

DesksTemplatesIconViewTestApi::~DesksTemplatesIconViewTestApi() = default;

DesksTemplatesItemView* GetItemViewFromTemplatesGrid(int grid_item_index) {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
  DCHECK(grid_widget);

  const DesksTemplatesGridView* templates_grid_view =
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
  DCHECK(templates_grid_view);

  std::vector<DesksTemplatesItemView*> grid_items =
      templates_grid_view->grid_items();
  DesksTemplatesItemView* item_view = grid_items.at(grid_item_index);
  DCHECK(item_view);
  return item_view;
}

views::Button* GetZeroStateDesksTemplatesButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view ? desks_bar_view->zero_state_desks_templates_button()
                        : nullptr;
}

views::Button* GetExpandedStateDesksTemplatesButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view
             ? desks_bar_view->expanded_state_desks_templates_button()
                   ->inner_button()
             : nullptr;
}

views::Button* GetSaveDeskAsTemplateButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;
  return overview_grid->GetSaveDeskAsTemplateButton();
}

views::Button* GetTemplateItemButton(int index) {
  auto* item = GetItemViewFromTemplatesGrid(index);
  return item ? static_cast<views::Button*>(item) : nullptr;
}

views::Button* GetTemplateItemDeleteButton(int index) {
  auto* item = GetItemViewFromTemplatesGrid(index);
  return item ? static_cast<views::Button*>(const_cast<CloseButton*>(
                    DesksTemplatesItemViewTestApi(item).delete_button()))
              : nullptr;
}

views::Button* GetDesksTemplatesDialogAcceptButton() {
  const views::Widget* dialog_widget =
      DesksTemplatesDialogController::Get()->dialog_widget();
  if (!dialog_widget)
    return nullptr;
  return dialog_widget->widget_delegate()->AsDialogDelegate()->GetOkButton();
}

void WaitForDesksTemplatesUI() {
  auto* overview_session = GetOverviewSession();
  DCHECK(overview_session);

  base::RunLoop run_loop;
  DesksTemplatesPresenterTestApi(overview_session->desks_templates_presenter())
      .SetOnUpdateUiClosure(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace ash
