// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_test_util.h"

#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/widget/widget_delegate.h"

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
    animator_->AddObserver(this);
  }

  BoundsAnimatorWaiter(const BoundsAnimatorWaiter&) = delete;
  BoundsAnimatorWaiter& operator=(const BoundsAnimatorWaiter&) = delete;

  ~BoundsAnimatorWaiter() override { animator_->RemoveObserver(this); }

  void Wait() {
    if (!animator_->IsAnimating()) {
      return;
    }

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

  const raw_ref<views::BoundsAnimator> animator_;
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
  for (ash::SavedDeskGridView* grid_view : library_view_->grid_views()) {
    SavedDeskGridViewTestApi(grid_view).WaitForItemMoveAnimationDone();
  }
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
  for (views::View* icon_view : item_view_->icon_container_view_->children()) {
    casted_icon_views.push_back(static_cast<SavedDeskIconView*>(icon_view));
  }
  return casted_icon_views;
}

SavedDeskItemHoverState SavedDeskItemViewTestApi::GetHoverState() const {
  float hover_layer_opacity =
      item_view_->hover_container_->layer()->GetTargetOpacity();
  float icon_layer_opacity =
      item_view_->icon_container_view_->layer()->GetTargetOpacity();

  if (hover_layer_opacity == 1.0f && icon_layer_opacity == 0.0f) {
    return SavedDeskItemHoverState::kHover;
  }
  if (hover_layer_opacity == 0.0f && icon_layer_opacity == 1.0f) {
    return SavedDeskItemHoverState::kIcons;
  }
  return SavedDeskItemHoverState::kIndeterminate;
}

SavedDeskIconViewTestApi::SavedDeskIconViewTestApi(
    const SavedDeskIconView* saved_desk_icon_view)
    : saved_desk_icon_view_(saved_desk_icon_view) {
  DCHECK(saved_desk_icon_view_);
}

SavedDeskIconViewTestApi::~SavedDeskIconViewTestApi() = default;

SavedDeskControllerTestApi::SavedDeskControllerTestApi(
    SavedDeskController* saved_desk_controller)
    : saved_desk_controller_(saved_desk_controller) {}

SavedDeskControllerTestApi::~SavedDeskControllerTestApi() = default;

void SavedDeskControllerTestApi::SetAdminTemplate(
    std::unique_ptr<DeskTemplate> admin_template) {
  saved_desk_controller_->SetAdminTemplateForTesting(std::move(admin_template));
}

void SavedDeskControllerTestApi::ResetAutoLaunch() {
  saved_desk_controller_->ResetAutoLaunchForTesting();
}

std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    OverviewGrid* overview_grid) {
  SavedDeskLibraryView* saved_desk_library_view =
      overview_grid->GetSavedDeskLibraryView();
  return GetItemViewsFromDeskLibrary(saved_desk_library_view);
}

std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    SavedDeskLibraryView* saved_desk_library_view) {
  DCHECK(saved_desk_library_view);
  std::vector<SavedDeskItemView*> grid_items;
  for (ash::SavedDeskGridView* grid_view :
       saved_desk_library_view->grid_views()) {
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

const views::Button* GetLibraryButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  if (!desks_bar_view) {
    return nullptr;
  }

  return desks_bar_view->library_button();
}

const views::Button* GetSaveDeskAsTemplateButton() {
  auto* overview_grid = GetPrimaryOverviewGrid();
  return overview_grid ? overview_grid->GetSaveDeskAsTemplateButton() : nullptr;
}

const views::Button* GetSaveDeskForLaterButton() {
  auto* overview_grid = GetPrimaryOverviewGrid();
  return overview_grid ? overview_grid->GetSaveDeskForLaterButton() : nullptr;
}

const views::Button* GetSavedDeskItemButton(int index) {
  auto* item = GetItemViewFromSavedDeskGrid(index);
  return item ? static_cast<views::Button*>(item) : nullptr;
}

const views::Button* GetSavedDeskItemDeleteButton(int index) {
  auto* item = GetItemViewFromSavedDeskGrid(index);
  return item ? const_cast<IconButton*>(
                    SavedDeskItemViewTestApi(item).delete_button())
              : nullptr;
}

const views::Button* GetSavedDeskDialogAcceptButton() {
  if (auto* dialog_widget_view = saved_desk_util::GetSavedDeskDialogController()
                                     ->GetSystemDialogViewForTesting()) {
    return dialog_widget_view->GetAcceptButtonForTesting();
  }
  return nullptr;
}

void WaitForSavedDeskUI() {
  auto* overview_session = GetOverviewSession();
  DCHECK(overview_session);

  base::RunLoop run_loop;
  SavedDeskPresenterTestApi(overview_session->saved_desk_presenter())
      .SetOnUpdateUiClosure(run_loop.QuitClosure());
  run_loop.Run();
}

bool WaitForLibraryButtonVisible() {
  return base::test::RunUntil(
      []() { return IsLazyInitViewVisible(GetLibraryButton()); });
}

const app_restore::AppRestoreData* QueryRestoreData(
    const DeskTemplate& saved_desk,
    std::optional<std::string> app_id,
    std::optional<int32_t> window_id) {
  const auto& app_id_to_launch_list =
      saved_desk.desk_restore_data()->app_id_to_launch_list();

  auto app_it = app_id ? app_id_to_launch_list.find(*app_id)
                       : app_id_to_launch_list.begin();
  if (app_it == app_id_to_launch_list.end()) {
    // No matching app found, or the app list is empty.
    return nullptr;
  }

  const auto& launch_list = app_it->second;
  auto window_it =
      window_id ? launch_list.find(*window_id) : launch_list.begin();
  if (window_it == launch_list.end()) {
    // No matching window found, or the window list is is empty.
    return nullptr;
  }

  return window_it->second.get();
}

void AddSavedDeskEntry(desks_storage::DeskModel* desk_model,
                       std::unique_ptr<DeskTemplate> saved_desk) {
  base::RunLoop loop;
  desk_model->AddOrUpdateEntry(
      std::move(saved_desk),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            CHECK_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                     status);
            loop.Quit();
          }));
  loop.Run();
}

void AddSavedDeskEntry(desks_storage::DeskModel* desk_model,
                       const base::Uuid& uuid,
                       const std::string& name,
                       base::Time created_time,
                       DeskTemplateSource source,
                       DeskTemplateType type,
                       std::unique_ptr<app_restore::RestoreData> restore_data) {
  auto saved_desk =
      std::make_unique<DeskTemplate>(uuid, source, name, created_time, type);
  saved_desk->set_desk_restore_data(std::move(restore_data));

  AddSavedDeskEntry(desk_model, std::move(saved_desk));
}

void AddSavedDeskEntry(desks_storage::DeskModel* desk_model,
                       const base::Uuid& uuid,
                       const std::string& name,
                       base::Time created_time,
                       DeskTemplateType type) {
  AddSavedDeskEntry(desk_model, uuid, name, created_time,
                    DeskTemplateSource::kUser, type,
                    std::make_unique<app_restore::RestoreData>());
}

}  // namespace ash
