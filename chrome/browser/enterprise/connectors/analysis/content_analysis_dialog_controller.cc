// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"

#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/textarea/textarea.h"

#include "base/win/windows_h_disallowed.h"

namespace enterprise_connectors {

namespace {

// These time values are non-const in order to be overridden in test so they
// complete faster.
base::TimeDelta minimum_pending_dialog_time_ = base::Seconds(2);
base::TimeDelta success_dialog_timeout_ = base::Seconds(1);
base::TimeDelta show_dialog_delay_ = base::Seconds(1);

ContentAnalysisDialogController::TestObserver* observer_for_testing = nullptr;

}  // namespace

// static
base::TimeDelta ContentAnalysisDialogController::GetMinimumPendingDialogTime() {
  return minimum_pending_dialog_time_;
}

// static
base::TimeDelta ContentAnalysisDialogController::GetSuccessDialogTimeout() {
  return success_dialog_timeout_;
}

// static
base::TimeDelta ContentAnalysisDialogController::ShowDialogDelay() {
  return show_dialog_delay_;
}

ContentAnalysisDialogController::ContentAnalysisDialogController(
    std::unique_ptr<ContentAnalysisDelegateBase> delegate,
    bool is_cloud,
    content::WebContents* contents,
    safe_browsing::DeepScanAccessPoint access_point,
    int files_count,
    FinalContentAnalysisResult final_result,
    download::DownloadItem* download_item)
    : ContentAnalysisDialogDelegate(delegate.get(),
                                    CreateWebContentsGetter(),
                                    is_cloud,
                                    access_point,
                                    files_count),
      content::WebContentsObserver(contents),
      delegate_base_(std::move(delegate)),
      download_item_(download_item) {
  DVLOG(1) << __func__;
  DCHECK(delegate_base_);

  // TODO(crbug.com/422111748): Move this to the code that initializes the
  // DialogDelegate once this class no longer inherits from it.
  final_result_ = final_result;
  SetOwnedByWidget(OwnedByWidgetPassKey());
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  DialogDelegate::SetAcceptCallback(
      base::BindOnce(&ContentAnalysisDialogController::AcceptButtonCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  DialogDelegate::SetCancelCallback(
      base::BindOnce(&ContentAnalysisDialogController::CancelButtonCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  if (observer_for_testing) {
    observer_for_testing->ConstructorCalled(this, base::TimeTicks::Now());
  }

  if (final_result_ != FinalContentAnalysisResult::SUCCESS) {
    UpdateStateFromFinalResult(final_result_);
  }

  SetupButtons();

  if (download_item_) {
    download_item_->AddObserver(this);
  }

  // Because the display of the dialog is delayed, it won't block UI
  // interaction with the top level web contents until it is visible.  To block
  // interaction as of now, ignore input events manually.
  top_level_contents_ =
      constrained_window::GetTopLevelWebContents(web_contents())->GetWeakPtr();

  top_level_contents_->StoreFocus();
  scoped_ignore_input_events_ =
      top_level_contents_->IgnoreInputEvents(std::nullopt);

  if (ShowDialogDelay().is_zero() || !is_pending()) {
    DVLOG(1) << __func__ << ": Showing in ctor";
    ShowDialogNow();
  } else {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContentAnalysisDialogController::ShowDialogNow,
                       weak_ptr_factory_.GetWeakPtr()),
        ShowDialogDelay());
  }

  if (is_warning() && bypass_requires_justification()) {
    bypass_justification_text_length_->SetEnabledColor(
        bypass_justification_text_length_->GetColorProvider()->GetColor(
            ui::kColorAlertHighSeverity));
  }
}

void ContentAnalysisDialogController::ShowDialogNow() {
  if (will_be_deleted_soon_) {
    DVLOG(1) << __func__ << ": aborting since dialog will be deleted soon";
    return;
  }

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (!manager) {
    // `manager` being null indicates that `web_contents()` doesn't correspond
    // to a browser tab (ex: an extension background page reading the
    // clipboard). In such a case, we don't show a dialog and instead simply
    // accept/cancel the result immediately. See crbug.com/374120523 and
    // crbug.com/388049470 for more context.
    if (!is_pending()) {
      CancelButtonCallback();
    }
    return;
  }

  // If the web contents is still valid when the delay timer goes off and the
  // dialog has not yet been shown, show it now.
  if (web_contents() && !contents_view_) {
    DVLOG(1) << __func__ << ": first time";
    first_shown_timestamp_ = base::TimeTicks::Now();
    constrained_window::ShowWebModalDialogViews(this, web_contents());
    if (observer_for_testing) {
      observer_for_testing->ViewsFirstShown(this, first_shown_timestamp_);
    }
  }
}

void ContentAnalysisDialogController::AcceptButtonCallback() {
  DCHECK(delegate_base_);
  DCHECK(is_warning());
  accepted_or_cancelled_ = true;
  std::optional<std::u16string> justification = std::nullopt;
  if (delegate_base_->BypassRequiresJustification() && bypass_justification_) {
    justification = bypass_justification_->GetText();
  }
  delegate_base_->BypassWarnings(justification);
}

void ContentAnalysisDialogController::CancelButtonCallback() {
  accepted_or_cancelled_ = true;
  if (delegate_base_) {
    delegate_base_->Cancel(is_warning());
  }
}

void ContentAnalysisDialogController::SuccessCallback() {
#if defined(USE_AURA)
  if (web_contents()) {
    // It's possible focus has been lost and gained back incorrectly if the user
    // clicked on the page between the time the scan started and the time the
    // dialog closes. This results in the behaviour detailed in
    // crbug.com/1139050. The fix is to preemptively take back focus when this
    // dialog closes on its own.
    scoped_ignore_input_events_.reset();
    web_contents()->Focus();
  }
#endif
}

void ContentAnalysisDialogController::WebContentsDestroyed() {
  // If WebContents are destroyed, then the scan results don't matter so the
  // delegate can be destroyed as well.
  CancelDialogWithoutCallback();
}

void ContentAnalysisDialogController::PrimaryPageChanged(content::Page& page) {
  // If the primary page is changed, the scan results would be stale. So the
  // delegate should be reset and dialog should be cancelled.
  CancelDialogWithoutCallback();
}

void ContentAnalysisDialogController::ShowResult(
    FinalContentAnalysisResult result) {
  DCHECK(is_pending());

  UpdateStateFromFinalResult(result);

  // Update the pending dialog only after it has been shown for a minimum amount
  // of time.
  base::TimeDelta time_shown = base::TimeTicks::Now() - first_shown_timestamp_;
  if (time_shown >= GetMinimumPendingDialogTime()) {
    UpdateDialog();
  } else {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContentAnalysisDialogController::UpdateDialog,
                       weak_ptr_factory_.GetWeakPtr()),
        GetMinimumPendingDialogTime() - time_shown);
  }
}

ContentAnalysisDialogController::~ContentAnalysisDialogController() {
  DVLOG(1) << __func__;

  // TODO(crbug.com/422111748): Update this cleanup code when this class stops
  // inheriting from ContentAnalysisDialogDelegate.
  ContentAnalysisDialogDelegate::delegate_base_ = nullptr;

  if (bypass_justification_) {
    bypass_justification_->SetController(nullptr);
  }

  if (top_level_contents_) {
    scoped_ignore_input_events_.reset();
    top_level_contents_->RestoreFocus();
  }
  if (download_item_) {
    download_item_->RemoveObserver(this);
  }
  if (observer_for_testing) {
    observer_for_testing->DestructorCalled(this);
  }
}

bool ContentAnalysisDialogController::ShouldShowDialogNow() {
  DCHECK(!is_pending());
  // If the final result is fail closed, display ui regardless of cloud or local
  // analysis.
  if (final_result_ == FinalContentAnalysisResult::FAIL_CLOSED) {
    DVLOG(1) << __func__ << ": show fail-closed ui.";
    return true;
  }
  // Otherwise, show dialog now only if it is cloud analysis and the verdict is
  // not success.
  return is_cloud_ && !is_success();
}

void ContentAnalysisDialogController::UpdateDialog() {
  if (!contents_view_ && !is_pending()) {
    // If the dialog is no longer pending, a final verdict was received before
    // the dialog was displayed.  Show the verdict right away only if
    // ShouldShowDialogNow() returns true.
    ShouldShowDialogNow() ? ShowDialogNow() : CancelDialogAndDelete();
    return;
  }

  UpdateDialogAppearance();

  // Schedule the dialog to close itself in the success case.
  if (is_success()) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DialogDelegate::CancelDialog,
                       weak_ptr_factory_.GetWeakPtr()),
        GetSuccessDialogTimeout());
  }

  if (observer_for_testing) {
    observer_for_testing->DialogUpdated(this, final_result_);
  }

  // Cancel the dialog as it is updated in tests in the failure dialog case.
  // This is necessary to terminate tests that end when the dialog is closed.
  if (observer_for_testing && is_failure()) {
    CancelDialog();
  }
}

void ContentAnalysisDialogController::CancelDialogAndDelete() {
  if (observer_for_testing) {
    observer_for_testing->CancelDialogAndDeleteCalled(this, final_result_);
  }

  if (contents_view_) {
    DVLOG(1) << __func__ << ": dialog will be canceled";
    CancelDialog();
  } else {
    DVLOG(1) << __func__ << ": dialog will be deleted soon";
    will_be_deleted_soon_ = true;
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }
}

// static
void ContentAnalysisDialogController::SetMinimumPendingDialogTimeForTesting(
    base::TimeDelta delta) {
  minimum_pending_dialog_time_ = delta;
}

// static
void ContentAnalysisDialogController::SetSuccessDialogTimeoutForTesting(
    base::TimeDelta delta) {
  success_dialog_timeout_ = delta;
}

// static
void ContentAnalysisDialogController::SetShowDialogDelayForTesting(
    base::TimeDelta delta) {
  show_dialog_delay_ = delta;
}

// static
void ContentAnalysisDialogController::SetObserverForTesting(
    TestObserver* observer) {
  observer_for_testing = observer;
}

views::ImageView* ContentAnalysisDialogController::GetTopImageForTesting()
    const {
  return image_;
}

views::Throbber* ContentAnalysisDialogController::GetSideIconSpinnerForTesting()
    const {
  return side_icon_spinner_;
}

views::StyledLabel* ContentAnalysisDialogController::GetMessageForTesting()
    const {
  return message_;
}

views::Link* ContentAnalysisDialogController::GetLearnMoreLinkForTesting()
    const {
  return learn_more_link_;
}

views::Label*
ContentAnalysisDialogController::GetBypassJustificationLabelForTesting() const {
  return justification_text_label_;
}

views::Textarea*
ContentAnalysisDialogController::GetBypassJustificationTextareaForTesting()
    const {
  return bypass_justification_;
}

views::Label*
ContentAnalysisDialogController::GetJustificationTextLengthForTesting() const {
  return bypass_justification_text_length_;
}

void ContentAnalysisDialogController::OnDownloadUpdated(
    download::DownloadItem* download) {
  if (download->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED &&
      !accepted_or_cancelled_) {
    // The user validated the verdict in another instance of
    // `ContentAnalysisDialogController`, so this one is now pointless and can
    // go away.
    CancelDialogWithoutCallback();
  }
}

void ContentAnalysisDialogController::OnDownloadOpened(
    download::DownloadItem* download) {
  if (!accepted_or_cancelled_) {
    CancelDialogWithoutCallback();
  }
}

void ContentAnalysisDialogController::OnDownloadDestroyed(
    download::DownloadItem* download) {
  if (!accepted_or_cancelled_) {
    CancelDialogWithoutCallback();
  }
  download_item_ = nullptr;
}

void ContentAnalysisDialogController::CancelDialogWithoutCallback() {
  // TODO(crbug.com/422111748): Update this cleanup code when this class stops
  // inheriting from ContentAnalysisDialogDelegate.
  ContentAnalysisDialogDelegate::delegate_base_ = nullptr;

  // Reset `delegate` so no logic runs when the dialog is cancelled.
  delegate_base_.reset(nullptr);

  // view may be null if the dialog was delayed and never shown before the
  // verdict is known.
  if (contents_view_) {
    CancelDialog();
  }
}

content::WebContents::Getter
ContentAnalysisDialogController::CreateWebContentsGetter() {
  return base::BindRepeating(&ContentAnalysisDialogController::web_contents,
                             base::Unretained(this));
}

}  // namespace enterprise_connectors
