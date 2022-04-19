// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_TASK_FEEDBACK_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_TASK_FEEDBACK_DIALOG_H_

#include <memory>

#include "ash/app_list/views/continue_task_view.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Button;
class Checkbox;
class RadioButton;
}  // namespace views

namespace ash {
class ContinueTaskView;
class ViewShadow;

// RemoveTaskFeedbackDialog displays a dialog for collecting feedback on users'
// reasons to remove an element from the continue section.
class RemoveTaskFeedbackDialog : public views::WidgetDelegateView {
 public:
  // The different combinations of feedback that could be given on this dialog.
  // These values are used for metrics and should not be changed.
  enum class FeedbackBuckets {
    // This bucket reflects that there was an error with the feedback provided.
    kInvalidFeedback = 0,
    kLocalFileDontWantAny = 1,
    kLocalFileDontWantThis = 2,
    kLocalFileDontNeed = 3,
    kLocalFileDontSee = 4,
    kLocalFileDontNeedDontSee = 5,
    kDriveFileDontWantAny = 6,
    kDriveFileDontWantThis = 7,
    kDriveFileDontNeed = 8,
    kDriveFileDontSee = 9,
    kDriveFileDontNeedDontSee = 10,
    kMaxValue = kDriveFileDontNeedDontSee,
  };

  using ConfirmDialogCallback = base::OnceCallback<void(bool)>;

  METADATA_HEADER(RemoveTaskFeedbackDialog);

  // Receives a callback to notify user's confirmation for removing the task
  // suggestion. Invoked only after the user confirms removing the suggestion by
  // clicking the Remove button. If the user cancels the dialog, callback will
  // not run.
  RemoveTaskFeedbackDialog(ConfirmDialogCallback callback,
                           ContinueTaskView::TaskResultType type);

  RemoveTaskFeedbackDialog(const RemoveTaskFeedbackDialog&) = delete;
  RemoveTaskFeedbackDialog& operator=(const RemoveTaskFeedbackDialog&) = delete;

  ~RemoveTaskFeedbackDialog() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  views::Button* cancel_button_for_test() { return cancel_button_; }
  views::Button* remove_button_for_test() { return remove_button_; }
  views::RadioButton* all_suggestions_option_for_test() {
    return all_suggestions_option_;
  }
  views::RadioButton* single_suggestion_option_for_test() {
    return single_suggestion_option_;
  }
  views::View* secondary_options_panel_for_test() {
    return secondary_options_panel_;
  }

 private:
  // Callbacks for dialog buttons.
  void Remove();
  void Cancel();

  // Invoked when `primary_options_second_` changes checked state.
  void ToggleSecondaryOptionsPanel();

  // Helpers to record the UMA for the dialog resuslts.
  RemoveTaskFeedbackDialog::FeedbackBuckets GetFeedbackBucketValue();
  void LogMetricsOnFeedbackSubmitted();

  views::Label* title_ = nullptr;
  views::Label* feedback_text_ = nullptr;

  // Primary feedback options. Indicates user preference regarding what
  // suggestions they would not have shown. Detailed feedback may be given as
  // selected.
  views::RadioButton* all_suggestions_option_ = nullptr;
  views::RadioButton* single_suggestion_option_ = nullptr;

  // View with detailed feedback options. Shown when user checks
  // `this_suggestion_option_`.
  views::View* secondary_options_panel_ = nullptr;
  // Detailed feedback options.
  views::Checkbox* done_using_option_ = nullptr;
  views::Checkbox* not_show_option_ = nullptr;

  views::Button* cancel_button_ = nullptr;
  views::Button* remove_button_ = nullptr;
  std::unique_ptr<ViewShadow> view_shadow_;

  ConfirmDialogCallback confirm_callback_;
  const ContinueTaskView::TaskResultType task_type_;

  base::CallbackListSubscription single_suggestion_option_subscription_;
};

BEGIN_VIEW_BUILDER(, RemoveTaskFeedbackDialog, views::WidgetDelegateView)
VIEW_BUILDER_OVERLOAD_METHOD_CLASS(views::WidgetDelegate,
                                   SetModalType,
                                   ui::ModalType)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(, ash::RemoveTaskFeedbackDialog)

#endif  // ASH_APP_LIST_VIEWS_REMOVE_TASK_FEEDBACK_DIALOG_H_
