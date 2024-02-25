// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/continue_task_container_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace ash {

class AppListNudgeController;
class AppListViewDelegate;
class AppListToastView;
class ContinueTaskContainerView;
class ContinueTaskView;

// The "Continue" section of the bubble launcher. This view wraps around
// suggestions with tasks to continue.
class ASH_EXPORT ContinueSectionView : public views::View,
                                       public views::FocusChangeListener,
                                       public AppListModelProvider::Observer,
                                       public AppListControllerObserver {
  METADATA_HEADER(ContinueSectionView, views::View)

 public:
  ContinueSectionView(AppListViewDelegate* view_delegate,
                      int columns,
                      bool tablet_mode);
  ContinueSectionView(const ContinueSectionView&) = delete;
  ContinueSectionView& operator=(const ContinueSectionView&) = delete;
  ~ContinueSectionView() override;

  // Returns true if the continue section removal metrics should be logged.
  static bool EnableContinueSectionFileRemovalMetrics();

  // Reset for the continue section file removal metric logging enabling.
  static void ResetContinueSectionFileRemovalMetricEnabledForTest();

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

  // Refresh the continue section element's visibility such as the privacy
  // notice, the continue label and the continue section itself.
  void UpdateElementsVisibility();

  ContinueTaskContainerView* suggestions_container() {
    return suggestions_container_;
  }

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // Sets the available width for the privacy toast view, so the privacy toast
  // preferred size fits within `available_width` of available horizontal space.
  void ConfigureLayoutForAvailableWidth(int available_width);

  AppListNudgeController* nudge_controller_for_test() const {
    return nudge_controller_;
  }

  // Fire `privacy_notice_shown_timer_` for testing purposes.
  bool FirePrivacyNoticeShownTimerForTest();

  AppListToastView* GetPrivacyNoticeForTest() const { return privacy_toast_; }

 private:
  // Whether there are a sufficient number of files to display the
  // section.
  bool HasMinimumFilesToShow() const;

  // Whether there is at least 1 admin template.
  bool HasDesksAdminTemplates() const;

  // Displays a toast with a privacy notice for the user in place of the
  // continue section. The user can accept the notice to display the continue
  // section in the launcher.
  void MaybeCreatePrivacyNotice();

  // Removes the privacy notice from the view.
  void RemovePrivacyNotice();

  // Invoked when the privacy notice has been shown for enough time.
  void OnPrivacyNoticeShowTimerDone();

  // Invoked when the privacy notice has been acknowledged.
  void OnPrivacyToastAcknowledged();

  // Starts the animation to dismiss the privacy notice toast.
  void AnimateDismissToast(base::RepeatingClosure callback);

  // Starts the animation to show the continue section in the app list bubble.
  void AnimateShowContinueSection();

  // Starts the animation for sliding other launcher content by
  // `vertical_offset`.
  void AnimateSlideLauncherContent(int vertical_offset);

  // Starts the animation to dismiss the privacy notice toast only. This is used
  // when the privacy notice does not have enough items after an update.
  void MaybeAnimateOutPrivacyNotice();

  const raw_ptr<AppListViewDelegate> view_delegate_;

  // If set, the amount of horizontal space available for the continue section -
  // used to configure layout for the continue section privacy notice toast.
  std::optional<int> available_width_;

  bool tablet_mode_ = false;

  // Timer for marking the privacy notice as shown.
  base::OneShotTimer privacy_notice_shown_timer_;

  // Not owned.
  raw_ptr<AppListNudgeController, DanglingUntriaged> nudge_controller_ =
      nullptr;

  raw_ptr<AppListToastView, DanglingUntriaged> privacy_toast_ = nullptr;
  raw_ptr<ContinueTaskContainerView> suggestions_container_ = nullptr;

  base::WeakPtrFactory<ContinueSectionView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
