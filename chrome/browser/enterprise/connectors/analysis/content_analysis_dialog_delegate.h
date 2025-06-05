// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"
#include "components/enterprise/connectors/core/common.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class BoxLayoutView;
}  // namespace views

namespace enterprise_connectors {

// Implementation of `views::DialogDelegate` used to show a user the state of
// content analysis triggered by one of their action.
class ContentAnalysisDialogDelegate : public views::DialogDelegate {
 public:
  // Enum used to represent what the dialog is currently showing.
  enum class State {
    // The dialog is shown with an explanation that the scan is being performed
    // and that the result is pending.
    PENDING,

    // The dialog is shown with a short message indicating that the scan was a
    // success and that the user may proceed with their upload, drag-and-drop or
    // paste.
    SUCCESS,

    // The dialog is shown with a message indicating that the scan was a failure
    // and that the user may not proceed with their upload, drag-and-drop or
    // paste.
    FAILURE,

    // The dialog is shown with a message indicating that the scan was a
    // failure, but that the user may proceed with their upload, drag-and-drop
    // or paste if they want to.
    WARNING,
  };

  explicit ContentAnalysisDialogDelegate(ContentAnalysisDelegateBase* delegate);
  ~ContentAnalysisDialogDelegate() override;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  ui::mojom::ModalType GetModalType() const override;

  // Accessors to simplify `dialog_state_` checking.
  inline bool is_success() const { return dialog_state_ == State::SUCCESS; }
  inline bool is_failure() const { return dialog_state_ == State::FAILURE; }
  inline bool is_warning() const { return dialog_state_ == State::WARNING; }
  inline bool is_pending() const { return dialog_state_ == State::PENDING; }

  void UpdateStateFromFinalResult(FinalContentAnalysisResult final_result);

  // TODO(crbug.com/422111748): Change this to "private" after
  // `ContentAnalysisDialogController` no longer inherits from this class.
 protected:
  // Helper functions to set/get various parts of the dialog depending on the
  // values of `dialog_state_` and `delegate_base_`.
  void SetupButtons();
  std::u16string GetCancelButtonText() const;

  raw_ptr<views::BoxLayoutView> contents_view_ = nullptr;

  // Used to show the appropriate message.
  FinalContentAnalysisResult final_result_;

  // Used to show the appropriate dialog depending on the scan's status.
  State dialog_state_ = State::PENDING;

  // Should be owned by the parent of this class.
  raw_ptr<ContentAnalysisDelegateBase> delegate_base_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_
