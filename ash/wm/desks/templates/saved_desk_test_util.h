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
#include "base/guid.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
class Button;
class Label;
}  // namespace views

namespace ash {

class CloseButton;
class OverviewGrid;
class PillButton;
class RoundedImageView;
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
  SavedDeskPresenter* const presenter_;
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
  SavedDeskLibraryView* library_view_;
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
  SavedDeskGridView* grid_view_;
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

  const CloseButton* delete_button() const {
    return item_view_->delete_button_;
  }

  const PillButton* launch_button() const { return item_view_->launch_button_; }

  const base::GUID uuid() const { return item_view_->desk_template_->uuid(); }

  const views::View* hover_container() const {
    return item_view_->hover_container_;
  }

  // Icons views are stored in the view hierarchy so this convenience function
  // returns them as a vector of SavedDeskIconView*.
  std::vector<SavedDeskIconView*> GetIconViews() const;

 private:
  const SavedDeskItemView* item_view_;
};

// Wrapper for `SavedDeskIconView` that exposes internal state to test
// functions.
class SavedDeskIconViewTestApi {
 public:
  explicit SavedDeskIconViewTestApi(
      const SavedDeskIconView* desks_templates_icon_view);
  SavedDeskIconViewTestApi(const SavedDeskIconViewTestApi&) = delete;
  SavedDeskIconViewTestApi& operator=(const SavedDeskIconViewTestApi&) = delete;
  ~SavedDeskIconViewTestApi();

  const views::Label* count_label() const {
    return desks_templates_icon_view_->count_label_;
  }

  const RoundedImageView* icon_view() const {
    return desks_templates_icon_view_->icon_view_;
  }

  const SavedDeskIconView* desks_templates_icon_view() const {
    return desks_templates_icon_view_;
  }

 private:
  const SavedDeskIconView* desks_templates_icon_view_;
};

// Returns all saved desk item views from the desk library on the given
// `overview_grid`.
std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    const OverviewGrid* overview_grid);

// Returns all saved desk item views from the given `saved_desk_library_view`.
std::vector<SavedDeskItemView*> GetItemViewsFromDeskLibrary(
    SavedDeskLibraryView* saved_desk_library_view);

// Return the `grid_item_index`th `SavedDeskItemView` from the first
// `OverviewGrid`'s `SavedDeskGridView` in `GetOverviewGridList()`.
SavedDeskItemView* GetItemViewFromSavedDeskGrid(size_t grid_item_index);

// These buttons are the ones on the primary root window.
views::Button* GetZeroStateLibraryButton();
views::Button* GetExpandedStateLibraryButton();
views::Button* GetSaveDeskAsTemplateButton();
views::Button* GetSaveDeskForLaterButton();
views::Button* GetTemplateItemButton(int index);
views::Button* GetTemplateItemDeleteButton(int index);
views::Button* GetSavedDeskDialogAcceptButton();

// A lot of the UI relies on calling into the local desk data manager to
// update, which sends callbacks via posting tasks. Call `WaitForSavedDeskUI()`
// if testing a piece of the UI which calls into the desk model.
void WaitForSavedDeskUI();

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_TEST_UTIL_H_
