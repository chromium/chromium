// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_test_util.h"

#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
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

SavedDeskPresenterTestApi::SavedDeskPresenterTestApi(
    SavedDeskPresenter* presenter)
    : presenter_(presenter) {
  DCHECK(presenter_);
}

SavedDeskPresenterTestApi::~SavedDeskPresenterTestApi() = default;

// static
void SavedDeskPresenterTestApi::WaitForSaveAndRecallBlockingDialog() {
  base::RunLoop loop;
  SavedDeskPresenter::SetModalDialogCallbackForTesting(loop.QuitClosure());
  loop.Run();
}

// static
void SavedDeskPresenterTestApi::FireWindowWatcherTimer() {
  SavedDeskPresenter::FireWindowWatcherTimerForTesting();
}

void SavedDeskPresenterTestApi::SetOnUpdateUiClosure(
    base::OnceClosure closure) {
  DCHECK(!presenter_->on_update_ui_closure_for_testing_);
  presenter_->on_update_ui_closure_for_testing_ = std::move(closure);
}

void SavedDeskPresenterTestApi::MaybeWaitForModel() {
  if (presenter_->weak_ptr_factory_.HasWeakPtrs())
    WaitForSavedDeskUI();
}

SavedDeskLibraryViewTestApi::SavedDeskLibraryViewTestApi(
    SavedDeskLibraryView* library_view)
    : library_view_(library_view) {}

void SavedDeskLibraryViewTestApi::WaitForAnimationDone() {
  for (auto* grid_view : library_view_->grid_views())
    SavedDeskGridViewTestApi(grid_view).WaitForItemMoveAnimationDone();
}

SavedDeskGridViewTestApi::SavedDeskGridViewTestApi(SavedDeskGridView* grid_view)
    : grid_view_(grid_view) {
  DCHECK(grid_view_);
}

SavedDeskGridViewTestApi::~SavedDeskGridViewTestApi() = default;

void SavedDeskGridViewTestApi::WaitForItemMoveAnimationDone() {
  BoundsAnimatorWaiter(grid_view_->bounds_animator_).Wait();
}

SavedDeskItemViewTestApi::SavedDeskItemViewTestApi(
    const SavedDeskItemView* item_view)
    : item_view_(item_view) {
  DCHECK(item_view_);
}

SavedDeskItemViewTestApi::~SavedDeskItemViewTestApi() = default;

std::vector<SavedDeskIconView*> SavedDeskItemViewTestApi::GetIconViews() const {
  std::vector<SavedDeskIconView*> casted_icon_views;
  for (auto* icon_view : item_view_->icon_container_view_->children()) {
    casted_icon_views.push_back(static_cast<SavedDeskIconView*>(icon_view));
  }
  return casted_icon_views;
}

SavedDeskIconViewTestApi::SavedDeskIconViewTestApi(
    const SavedDeskIconView* desks_templates_icon_view)
    : desks_templates_icon_view_(desks_templates_icon_view) {
  DCHECK(desks_templates_icon_view_);
}

SavedDeskIconViewTestApi::~SavedDeskIconViewTestApi() = default;

std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    const OverviewGrid* overview_grid) {
  SavedDeskLibraryView* saved_desk_library_view =
      overview_grid->GetSavedDeskLibraryView();
  return GetItemViewsFromDeskLibrary(saved_desk_library_view);
}

std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    SavedDeskLibraryView* saved_desk_library_view) {
  DCHECK(saved_desk_library_view);
  std::vector<SavedDeskItemView*> grid_items;
  for (auto* grid_view : saved_desk_library_view->grid_views()) {
    auto& items = grid_view->grid_items();
    grid_items.insert(grid_items.end(), items.begin(), items.end());
  }
  return grid_items;
}

SavedDeskItemView* GetItemViewFromSavedDeskGrid(size_t grid_item_index) {
  auto* overview_grid = GetPrimaryOverviewGrid();
  DCHECK(overview_grid);

  SavedDeskPresenterTestApi(
      overview_grid->overview_session()->saved_desk_presenter())
      .MaybeWaitForModel();

  auto grid_items = GetItemViewsFromDeskLibrary(overview_grid);
  DCHECK_LT(grid_item_index, grid_items.size());

  SavedDeskItemView* item_view = grid_items[grid_item_index];
  DCHECK(item_view);
  return item_view;
}

views::Button* GetZeroStateLibraryButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view ? desks_bar_view->zero_state_desks_templates_button()
                        : nullptr;
}

views::Button* GetExpandedStateLibraryButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view
             ? desks_bar_view->expanded_state_desks_templates_button()
                   ->GetInnerButton()
             : nullptr;
}

views::Button* GetSaveDeskAsTemplateButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;
  return overview_grid->GetSaveDeskAsTemplateButton();
}

views::Button* GetSaveDeskForLaterButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  return overview_grid ? overview_grid->GetSaveDeskForLaterButton() : nullptr;
}

views::Button* GetTemplateItemButton(int index) {
  auto* item = GetItemViewFromSavedDeskGrid(index);
  return item ? static_cast<views::Button*>(item) : nullptr;
}

views::Button* GetTemplateItemDeleteButton(int index) {
  auto* item = GetItemViewFromSavedDeskGrid(index);
  return item ? static_cast<views::Button*>(const_cast<CloseButton*>(
                    SavedDeskItemViewTestApi(item).delete_button()))
              : nullptr;
}

views::Button* GetSavedDeskDialogAcceptButton() {
  const views::Widget* dialog_widget =
      saved_desk_util::GetSavedDeskDialogController()->dialog_widget();
  if (!dialog_widget)
    return nullptr;
  return dialog_widget->widget_delegate()->AsDialogDelegate()->GetOkButton();
}

void WaitForSavedDeskUI() {
  auto* overview_session = GetOverviewSession();
  DCHECK(overview_session);

  base::RunLoop run_loop;
  SavedDeskPresenterTestApi(overview_session->saved_desk_presenter())
      .SetOnUpdateUiClosure(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace ash
