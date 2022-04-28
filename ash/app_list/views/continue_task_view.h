// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_

#include <memory>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/model/search/search_result_observer.h"
#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
class MenuRunner;
}  // namespace views

namespace ash {
class AppListViewDelegate;
class SearchResultPageDialogController;

enum ContinueTaskCommandId {
  // Context Menu option to open the selected suggestion.
  kOpenResult = 0,
  // Context Menu option to prevent the suggestion from showing.
  kRemoveResult = 1,
  // Context Menu option to hide the continue section.
  kHideContinueSection = 2,
};

// A view with a suggested task for the "Continue" section.
class ASH_EXPORT ContinueTaskView : public views::Button,
                                    public views::ContextMenuController,
                                    public ui::SimpleMenuModel::Delegate,
                                    public SearchResultObserver {
 public:
  // The type of result for the task.
  // These values are used for metrics and should not be changed.
  enum class TaskResultType {
    kLocalFile = 0,
    kDriveFile = 1,
    kUnknown = 2,
    kMaxValue = kUnknown,
  };

  METADATA_HEADER(ContinueTaskView);

  ContinueTaskView(AppListViewDelegate* view_delegate,
                   SearchResultPageDialogController* dialog_controller,
                   bool tablet_mode);
  ContinueTaskView(const ContinueTaskView&) = delete;
  ContinueTaskView& operator=(const ContinueTaskView&) = delete;
  ~ContinueTaskView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnThemeChanged() override;

  // SearchResultObserver:
  void OnResultDestroying() override;
  void OnMetadataChanged() override;

  void SetResult(SearchResult* result);

  // Returns true if the context menu for this task is showing.
  bool IsMenuShowing() const;

  void set_index_in_container(size_t index) { index_in_container_ = index; }
  SearchResult* result() const { return result_; }
  int index_in_container() const { return index_in_container_.value_or(-1); }

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // Returns the type of result for the task. Used for metrics.
  TaskResultType GetTaskResultType();

 private:
  void UpdateIcon();
  gfx::Size GetIconSize() const;
  void UpdateResult();

  void OnButtonPressed(const ui::Event& event);

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Opens the search result related to the view.
  void OpenResult(int event_flags);

  // Removes the search result related to the view.
  void RemoveResult();

  // Displays a dialog requesting for continue section feedback.
  void ShowFeedbackDialog();

  // Builds and returns a raw pointer to `context_menu_model_`.
  ui::SimpleMenuModel* BuildMenuModel();

  // Closes the context menu for this view if it is running.
  void CloseContextMenu();

  // Updates the background and the border if the ContinueTaskView is in tablet
  // mode.
  void UpdateStyleForTabletMode();

  // Record metrics at the moment when the ContinueTaskView result is removed.
  void LogMetricsOnResultRemoved();

  // Returns true if the feedback dialog should be shown on task removal.
  bool ShouldShowFeedbackDialog();

  // Invoked when the remove feedback dialog for the task has been closed by the
  // usser confirming the removal. This function handles removing the result and
  // updating the remove feedback dialog pref if |has_feedback| is true.
  void RemoveResultAndMaybeUpdateFeedbackPref(bool has_feedback);

  // The index of this view within a |SearchResultContainerView| that holds it.
  absl::optional<int> index_in_container_;

  AppListViewDelegate* const view_delegate_;
  views::Label* title_ = nullptr;
  views::Label* subtitle_ = nullptr;
  views::ImageView* icon_ = nullptr;
  SearchResult* result_ = nullptr;  // Owned by SearchModel::SearchResults.

  // Controller for displaying a searchbox anchored dialog.
  SearchResultPageDialogController* const dialog_controller_;

  const bool is_tablet_mode_;

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  base::ScopedObservation<SearchResult, SearchResultObserver>
      search_result_observation_{this};

  base::WeakPtrFactory<ContinueTaskView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_
