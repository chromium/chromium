// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/continue_task_container_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AppListNudgeController;
class AppListViewDelegate;
class AppListToastView;
class ContinueTaskContainerView;
class ContinueTaskView;

// The "Continue" section of the bubble launcher. This view wraps around
// suggestions with tasks to continue.
class ASH_EXPORT ContinueSectionView : public views::View,
                                       public AppListModelProvider::Observer,
                                       public AppListControllerObserver {
 public:
  METADATA_HEADER(ContinueSectionView);

  ContinueSectionView(AppListViewDelegate* view_delegate,
                      int columns,
                      bool tablet_mode);
  ContinueSectionView(const ContinueSectionView&) = delete;
  ContinueSectionView& operator=(const ContinueSectionView&) = delete;
  ~ContinueSectionView() override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // Called when the `suggestion_container_` finishes updating the tasks.
  void OnSearchResultContainerResultsChanged();

  // Schedule an update to the `suggestion_container_` tasks.
  void UpdateSuggestionTasks();

  size_t GetTasksSuggestionsCount() const;

  void DisableFocusForShowingActiveFolder(bool disabled);

  ContinueTaskView* GetTaskViewAtForTesting(size_t index) const;

  // Whether the privacy notice should be shown to the user.
  bool ShouldShowPrivacyNotice() const;

  // Whether the continue files should be shown to the user.
  bool ShouldShowFilesSection() const;

  // Stops the running `privacy_notice_shown_timer_` if the privacy notice is
  // shown in background.
  void SetShownInBackground(bool shown_in_background);

  void SetNudgeController(AppListNudgeController* nudge_controller);

  ContinueTaskContainerView* suggestions_container() {
    return suggestions_container_;
  }

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  AppListNudgeController* nudge_controller_for_test() const {
    return nudge_controller_;
  }

  static void SetPrivacyNoticeAcceptedForTest(bool is_disabled);

  // Fire `privacy_notice_shown_timer_` for testing purposes.
  bool FirePrivacyNoticeShownTimerForTest();

  AppListToastView* GetPrivacyNoticeForTest() const { return privacy_toast_; }

 private:
  // Whether there are a sufficient number of files to display the
  // section.
  bool HasMinimumFilesToShow() const;

  // Displays a toast with a privacy notice for the user in place of the
  // continue section. The user can accept the notice to display the continue
  // section in the launcher.
  void MaybeCreatePrivacyNotice();

  // Refresh the continue section element's visibility such as the privacy
  // notice, the continue label and the continue section itself.
  void UpdateElementsVisibility();

  // Invoked when the privacy notice has been accepted.
  void MarkPrivacyNoticeAccepted();

  // Invoked when the privacy notice has been shown for enough time.
  void MarkPrivacyNoticeShown();

  // Invoked after the `privacy_notice_count_timer_` fires.
  void OnPrivacyNoticeCountTimerDone();

  // Whether the user has already accepted the privacy notice.
  bool IsPrivacyNoticeAccepted() const;

  // Whether the user has already seen the privacy notice.
  bool IsPrivacyNoticeShown() const;

  bool tablet_mode_ = false;

  // Timer for marking the privacy notice as shown.
  base::OneShotTimer privacy_notice_shown_timer_;

  // Not owned.
  AppListNudgeController* nudge_controller_ = nullptr;

  views::Label* continue_label_ = nullptr;
  AppListToastView* privacy_toast_ = nullptr;
  ContinueTaskContainerView* suggestions_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
