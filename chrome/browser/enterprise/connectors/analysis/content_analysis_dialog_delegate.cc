// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_delegate.h"

#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout_view.h"

namespace enterprise_connectors {

ContentAnalysisDialogDelegate::ContentAnalysisDialogDelegate(
    ContentAnalysisDelegateBase* delegate)
    : delegate_base_(delegate) {}

ContentAnalysisDialogDelegate::~ContentAnalysisDialogDelegate() = default;

std::u16string ContentAnalysisDialogDelegate::GetWindowTitle() const {
  return std::u16string();
}

bool ContentAnalysisDialogDelegate::ShouldShowCloseButton() const {
  return false;
}

views::Widget* ContentAnalysisDialogDelegate::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* ContentAnalysisDialogDelegate::GetWidget() const {
  return contents_view_->GetWidget();
}

ui::mojom::ModalType ContentAnalysisDialogDelegate::GetModalType() const {
  return ui::mojom::ModalType::kChild;
}

void ContentAnalysisDialogDelegate::UpdateStateFromFinalResult(
    FinalContentAnalysisResult final_result) {
  final_result_ = final_result;
  switch (final_result_) {
    case FinalContentAnalysisResult::ENCRYPTED_FILES:
    case FinalContentAnalysisResult::LARGE_FILES:
    case FinalContentAnalysisResult::FAIL_CLOSED:
    case FinalContentAnalysisResult::FAILURE:
      dialog_state_ = State::FAILURE;
      break;
    case FinalContentAnalysisResult::SUCCESS:
      dialog_state_ = State::SUCCESS;
      break;
    case FinalContentAnalysisResult::WARNING:
      dialog_state_ = State::WARNING;
      break;
  }
}

void ContentAnalysisDialogDelegate::SetupButtons() {
  if (is_warning()) {
    // Include the Ok and Cancel buttons if there is a bypassable warning.
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kCancel) |
        static_cast<int>(ui::mojom::DialogButton::kOk));
    DialogDelegate::SetDefaultButton(
        static_cast<int>(ui::mojom::DialogButton::kCancel));

    DialogDelegate::SetButtonLabel(ui::mojom::DialogButton::kCancel,
                                   GetCancelButtonText());

    DialogDelegate::SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_PROCEED_BUTTON));

    if (delegate_base_->BypassRequiresJustification()) {
      DialogDelegate::SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
    }
  } else if (is_failure() || is_pending()) {
    // Include the Cancel button when the scan is pending or failing.
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kCancel));
    DialogDelegate::SetDefaultButton(
        static_cast<int>(ui::mojom::DialogButton::kNone));

    DialogDelegate::SetButtonLabel(ui::mojom::DialogButton::kCancel,
                                   GetCancelButtonText());
  } else {
    // Include no buttons otherwise.
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kNone));
  }
}

std::u16string ContentAnalysisDialogDelegate::GetCancelButtonText() const {
  int text_id;
  auto overriden_text = delegate_base_->OverrideCancelButtonText();
  if (overriden_text) {
    return overriden_text.value();
  }

  switch (dialog_state_) {
    case State::SUCCESS:
      NOTREACHED();
    case State::PENDING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_UPLOAD_BUTTON;
      break;
    case State::FAILURE:
      text_id = IDS_CLOSE;
      break;
    case State::WARNING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_WARNING_BUTTON;
      break;
  }
  return l10n_util::GetStringUTF16(text_id);
}

}  // namespace enterprise_connectors
