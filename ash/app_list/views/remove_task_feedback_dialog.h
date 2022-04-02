// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_TASK_FEEDBACK_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_TASK_FEEDBACK_DIALOG_H_

#include <memory>

#include "base/scoped_multi_source_observation.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Button;
class RadioButton;
}  // namespace views

namespace ash {
class ViewShadow;

// RemoveTaskFeedbackDialog displays a dialog for collecting feedback on users'
// reasons to remove an element from the continue section.
class RemoveTaskFeedbackDialog : public views::WidgetDelegateView {
 public:
  METADATA_HEADER(RemoveTaskFeedbackDialog);

  // Receives a callback to notify user's confirmation for removing the task
  // suggestion. Invoked only after the user confirms removing the suggestion by
  // clicking the Remove button. If the user cancels the dialog, callback will
  // not run.
  explicit RemoveTaskFeedbackDialog(base::OnceClosure callback);

  RemoveTaskFeedbackDialog(const RemoveTaskFeedbackDialog&) = delete;
  RemoveTaskFeedbackDialog& operator=(const RemoveTaskFeedbackDialog&) = delete;

  ~RemoveTaskFeedbackDialog() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  views::Button* cancel_button_for_test() { return cancel_button_; }
  views::Button* remove_button_for_test() { return remove_button_; }

 private:
  // Callbacks for dialog buttons.
  void Remove();
  void Cancel();

  // Invoked when `secondary_options_control_` changes checked state.
  void ToggleSecondaryOptionsPanel();

  // Toggles the visibility of `secondary_options_panel_`.
  views::RadioButton* secondary_options_control_ = nullptr;
  // View with more feedback options.
  views::View* secondary_options_panel_ = nullptr;
  views::Button* cancel_button_ = nullptr;
  views::Button* remove_button_ = nullptr;
  std::unique_ptr<ViewShadow> view_shadow_;

  base::OnceClosure confirm_callback_;
  base::CallbackListSubscription secondary_options_control_subscription_;
};

BEGIN_VIEW_BUILDER(, RemoveTaskFeedbackDialog, views::WidgetDelegateView)
VIEW_BUILDER_OVERLOAD_METHOD_CLASS(views::WidgetDelegate,
                                   SetModalType,
                                   ui::ModalType)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(, ash::RemoveTaskFeedbackDialog)

#endif  // ASH_APP_LIST_VIEWS_REMOVE_TASK_FEEDBACK_DIALOG_H_
