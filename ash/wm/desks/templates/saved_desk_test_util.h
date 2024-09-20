// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_TEST_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_TEST_UTIL_H_

#include <vector>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/desks_storage/core/desk_model.h"
#include "ui/views/controls/scroll_view.h"

namespace app_restore {
struct AppRestoreData;
}  // namespace app_restore

namespace ash {

class IconButton;
class OverviewGrid;
class PillButton;
class SavedDeskController;
class SavedDeskPresenter;

// Wrapper for `SavedDeskPresenter` that exposes internal state to test
// functions.
class SavedDeskPresenterTestApi {
 public:
  explicit SavedDeskPresenterTestApi(SavedDeskPresenter* presenter);
  SavedDeskPresenterTestApi(const SavedDeskPresenterTestApi&) = delete;
  SavedDeskPresenterTestApi& operator=(const SavedDeskPresenterTestApi&) =
      delete;
  ~SavedDeskPresenterTestApi();

  // When Save & Recall closes windows, they might object and present a blocking
  // dialog. This function waits until that dialog is detected.
  static void WaitForSaveAndRecallBlockingDialog();

  // Fire window watcher's timer to automatically transition into the saved desk
  // library. This will DCHECK if the timer is not active.
  static void FireWindowWatcherTimer();

  void SetOnUpdateUiClosure(base::OnceClosure closure);

  // If there are outstanding operations on the desk model, this blocks until
  // they are done.
  void MaybeWaitForModel();

 private:
  const raw_ptr<SavedDeskPresenter> presenter_;
};

// Wrapper for `SavedDeskLibraryView` that exposes internal state to test
// functions.
class SavedDeskLibraryViewTestApi {
 public:
  explicit SavedDeskLibraryViewTestApi(SavedDeskLibraryView* library_view);
  SavedDeskLibraryViewTestApi(SavedDeskLibraryViewTestApi&) = delete;
  SavedDeskLibraryViewTestApi& operator=(SavedDeskLibraryViewTestApi&) = delete;
  ~SavedDeskLibraryViewTestApi() = default;

  const views::ScrollView* scroll_view() { return library_view_->scroll_view_; }

  const views::Label* no_items_label() {
    return library_view_->no_items_label_;
  }

  void WaitForAnimationDone();

 private:
  raw_ptr<SavedDeskLibraryView, DanglingUntriaged> library_view_;
};

// Wrapper for `SavedDeskGridView` that exposes internal state to test
// functions.
class SavedDeskGridViewTestApi {
 public:
  explicit SavedDeskGridViewTestApi(SavedDeskGridView* grid_view);
  SavedDeskGridViewTestApi(SavedDeskGridViewTestApi&) = delete;
  SavedDeskGridViewTestApi& operator=(SavedDeskGridViewTestApi&) = delete;
  ~SavedDeskGridViewTestApi();

  void WaitForItemMoveAnimationDone();

 private:
  raw_ptr<SavedDeskGridView> grid_view_;
};

// Represents the visual state of a saved desk item - whether it is currently
// showing the icons, the hover container (the launch button) or is in some
// indeterminate state.
enum class SavedDeskItemHoverState {
  kIndeterminate,
  kIcons,  // Currently showing icons.
  kHover,  // Currently showing hover state.
};

// Wrapper for `SavedDeskItemView` that exposes internal state to test
// functions.
class SavedDeskItemViewTestApi {
 public:
  explicit SavedDeskItemViewTestApi(const SavedDeskItemView* item_view);
  SavedDeskItemViewTestApi(const SavedDeskItemViewTestApi&) = delete;
  SavedDeskItemViewTestApi& operator=(const SavedDeskItemViewTestApi&) = delete;
  ~SavedDeskItemViewTestApi();

  const views::Label* time_view() const { return item_view_->time_view_; }

  const IconButton* delete_button() const { return item_view_->delete_button_; }

  const PillButton* launch_button() const { return item_view_->launch_button_; }

  const base::Uuid uuid() const { return item_view_->saved_desk_->uuid(); }

  // Icons views are stored in the view hierarchy so this convenience function
  // returns them as a vector of SavedDeskIconView*.
  std::vector<SavedDeskIconView*> GetIconViews() const;

  SavedDeskItemHoverState GetHoverState() const;

 private:
  raw_ptr<const SavedDeskItemView> item_view_;
};

// Wrapper for `SavedDeskIconView` that exposes internal state to test
// functions.
class SavedDeskIconViewTestApi {
 public:
  explicit SavedDeskIconViewTestApi(
      const SavedDeskIconView* saved_desk_icon_view);
  SavedDeskIconViewTestApi(const SavedDeskIconViewTestApi&) = delete;
  SavedDeskIconViewTestApi& operator=(const SavedDeskIconViewTestApi&) = delete;
  ~SavedDeskIconViewTestApi();

  const views::Label* count_label() const {
    return saved_desk_icon_view_->count_label_;
  }

  const SavedDeskIconView* saved_desk_icon_view() const {
    return saved_desk_icon_view_;
  }

 private:
  raw_ptr<const SavedDeskIconView> saved_desk_icon_view_;
};

// Test API for `SavedDeskController`.
class SavedDeskControllerTestApi {
 public:
  explicit SavedDeskControllerTestApi(
      SavedDeskController* saved_desk_controller);
  SavedDeskControllerTestApi(const SavedDeskControllerTestApi&) = delete;
  SavedDeskControllerTestApi& operator=(const SavedDeskControllerTestApi&) =
      delete;
  ~SavedDeskControllerTestApi();

  void SetAdminTemplate(std::unique_ptr<DeskTemplate> admin_template);

  void ResetAutoLaunch();

 private:
  raw_ptr<SavedDeskController> saved_desk_controller_;
};

// Returns all saved desk item views from the desk library on the given
// `overview_grid`.
std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    OverviewGrid* overview_grid);

// Returns all saved desk item views from the given `saved_desk_library_view`.
std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    SavedDeskLibraryView* saved_desk_library_view);

// Returns the `grid_item_index`th `SavedDeskItemView` from the first
// `OverviewGrid`'s `SavedDeskGridView` in `GetOverviewGridList()`.
SavedDeskItemView* GetItemViewFromSavedDeskGrid(size_t grid_item_index);

// These buttons are the ones on the primary root window.
const views::Button* GetLibraryButton();
const views::Button* GetSaveDeskAsTemplateButton();
const views::Button* GetSaveDeskForLaterButton();
const views::Button* GetSavedDeskItemButton(int index);
const views::Button* GetSavedDeskItemDeleteButton(int index);
const views::Button* GetSavedDeskDialogAcceptButton();

// A lot of the UI relies on calling into the local desk data manager to
// update, which sends callbacks via posting tasks. Call `WaitForSavedDeskUI()`
// if testing a piece of the UI which calls into the desk model.
void WaitForSavedDeskUI();

// Waits until the library button is visible. Returns false if the library
// button doesn't become visible within a timeout.
bool WaitForLibraryButtonVisible();

// Retrieves the AppRestoreData (if any) from a template. For both `app_id` and
// `window_id`: if not set - the first occurrence is used. Returns nullptr if
// matching data is not found.
const app_restore::AppRestoreData* QueryRestoreData(
    const DeskTemplate& saved_desk,
    std::optional<std::string> app_id,
    std::optional<int32_t> window_id = {});

// Adds a captured desk entry to the desks model.
void AddSavedDeskEntry(desks_storage::DeskModel* desk_model,
                       std::unique_ptr<DeskTemplate> saved_desk);

void AddSavedDeskEntry(desks_storage::DeskModel* desk_model,
                       const base::Uuid& uuid,
                       const std::string& name,
                       base::Time created_time,
                       DeskTemplateSource source,
                       DeskTemplateType type,
                       std::unique_ptr<app_restore::RestoreData> restore_data);

// Adds an entry to the desks model directly without capturing a desk. Allows
// for testing the names and times of the UI directly.
void AddSavedDeskEntry(desks_storage::DeskModel* desk_model,
                       const base::Uuid& uuid,
                       const std::string& name,
                       base::Time created_time,
                       DeskTemplateType type);

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_TEST_UTIL_H_
