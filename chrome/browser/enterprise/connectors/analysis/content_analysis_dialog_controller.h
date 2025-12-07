// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_H_

#include <cstddef>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_delegate.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/connectors/core/content_analysis_delegate_base.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_connectors {

// Dialog shown for Deep Scanning to offer the possibility of cancelling the
// upload to the user.
class ContentAnalysisDialogController
    : public content::WebContentsObserver,
      public download::DownloadItem::Observer {
 public:
  // TestObserver should be implemented by tests that need to track when certain
  // ContentAnalysisDialogController functions are called. The test can add
  // itself as an observer by using SetObserverForTesting.
  class TestObserver {
   public:
    virtual ~TestObserver() = default;

    // Called at the start of ContentAnalysisDialogController's constructor.
    // `dialog` is a pointer to the newly constructed
    // ContentAnalysisDialogDelegate and should be kept in memory by the test
    // in order to validate its state.
    virtual void ConstructorCalled(ContentAnalysisDialogDelegate* dialog,
                                   base::TimeTicks timestamp) {}

    // Called at the end of ContentAnalysisDialogController::Show. `timestamp`
    // is the time used by ContentAnalysisDialogController to decide whether the
    // pending state has been shown for long enough. The test can keep this time
    // in memory and validate the pending time was sufficient in DialogUpdated.
    virtual void ViewsFirstShown(ContentAnalysisDialogDelegate* dialog,
                                 base::TimeTicks timestamp) {}

    // Called at the end of ContentAnalysisDialogController::UpdateDialog.
    // `result` is the value that UpdatedDialog used to transition from the
    // pending state to the success/failure/warning state.
    virtual void DialogUpdated(ContentAnalysisDialogDelegate* dialog,
                               FinalContentAnalysisResult result) {}

    // Called at the start of CancelDialogAndDelete(). `dialog` is a pointer
    // that will soon be destructed. Along with `result`, it is used by the test
    // to validate the dialog should be canceled or deleted.
    virtual void CancelDialogAndDeleteCalled(
        ContentAnalysisDialogDelegate* dialog,
        FinalContentAnalysisResult result) {}

    // Called at the end of ContentAnalysisDialogController's destructor.
    // `dialog` is a pointer to the ContentAnalysisDialogDelegate being
    // destructed. It can be used to compare it to the pointer obtained from
    // ConstructorCalled to ensure which view is being destroyed.
    virtual void DestructorCalled(ContentAnalysisDialogDelegate* dialog) {}
  };

  static void SetObserverForTesting(TestObserver* observer);

  static void SetMinimumPendingDialogTimeForTesting(base::TimeDelta delta);
  static void SetSuccessDialogTimeoutForTesting(base::TimeDelta delta);
  static void SetShowDialogDelayForTesting(base::TimeDelta delta);

  static base::TimeDelta GetMinimumPendingDialogTime();
  static base::TimeDelta GetSuccessDialogTimeout();
  static base::TimeDelta ShowDialogDelay();

  ContentAnalysisDialogController(
      std::unique_ptr<ContentAnalysisDelegateBase> delegate,
      bool is_cloud,
      content::WebContents* web_contents,
      DeepScanAccessPoint access_point,
      int files_count,
      FinalContentAnalysisResult final_result =
          FinalContentAnalysisResult::SUCCESS,
      download::DownloadItem* download_item = nullptr);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  // Updates the dialog with the result, and simply delete it from memory if
  // nothing should be shown.
  void ShowResult(FinalContentAnalysisResult result);

  // Cancels the dialog an schedules it for deletion if visible, otherwise
  // simply deletes it soon.
  void CancelDialogAndDelete();

  void CloseDialog(views::Widget::ClosedReason reason);

  ContentAnalysisDialogDelegate* dialog_delegate_for_testing();

 private:
  // Friend the unit test class for this so it can call the private dtor.
  friend class ContentAnalysisDialogPlainTest;

  // Friend to allow use of TaskRunner::DeleteSoon().
  friend class base::DeleteHelper<ContentAnalysisDialogController>;

  ~ContentAnalysisDialogController() override;

  // Callback function of delayed timer to make the dialog visible.
  void ShowDialogNow();

  // Update the UI depending on `dialog_state_`. This also triggers resizes and
  // fires some events. It's meant to be called to update the entire dialog when
  // it's already showing.
  // This function can only be called after the dialog widget is initialized.
  void UpdateDialog();

  // Helper function to determine whether dialog should be shown immediately.
  bool ShouldShowDialogNow();

  void AcceptButtonClicked();
  void CancelButtonClicked();

  // This callback used by DialogDelegate::SetCancelCallback and is used to
  // ensure the auto-closing success dialog handles focus correctly.
  void SuccessCallback();

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadOpened(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // Several conditions can lead to the dialog being no longer useful, so this
  // method is shared for those different conditions to close the dialog.
  void CancelDialogWithoutCallback();

  content::WebContents::Getter CreateWebContentsGetter();

  std::unique_ptr<ContentAnalysisDelegateBase> delegate_base_;

  base::TimeTicks first_shown_timestamp_;

  // `DownloadItem` for dialogs corresponding to a download with a reviewable
  // verdict. nullptr otherwise.
  raw_ptr<download::DownloadItem> download_item_ = nullptr;

  // Set to true once the dialog is either accepted or cancelled by the user.
  // This is used to decide whether the dialog should go away without user input
  // or not.
  bool accepted_or_cancelled_ = false;

  // Set to true once `DeleteSoon()` is called in `CancelDialogAndDelete()`.
  // This is used by other pending tasks, such as `ShowDialogNow()` to do
  // nothing if the dialog has been scheduled for deletion.
  bool will_be_deleted_soon_ = false;

  // If input events for our `WebContents` have been ignored, then this is the
  // closure to re-enable them.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  // A reference to the top level web contents of the tab whose content is
  // being analyzed.  Input events of this contents are ignored for the life
  // time of the dialog.
  base::WeakPtr<content::WebContents> top_level_contents_;

  std::unique_ptr<ContentAnalysisDialogDelegate> dialog_delegate_;
  std::unique_ptr<views::Widget> widget_;

  base::WeakPtrFactory<ContentAnalysisDialogController> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_H_
