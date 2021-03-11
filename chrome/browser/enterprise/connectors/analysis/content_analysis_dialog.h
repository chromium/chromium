// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
class Link;
class Throbber;
class Widget;
}  // namespace views

namespace enterprise_connectors {
class DeepScanningTopImageView;
class DeepScanningSideIconImageView;
class DeepScanningSideIconSpinnerView;
class DeepScanningMessageView;

// Dialog shown for Deep Scanning to offer the possibility of cancelling the
// upload to the user.
class ContentAnalysisDialog : public views::DialogDelegate,
                              public content::WebContentsObserver {
 public:
  // Enum used to represent what the dialog is currently showing.
  enum class DeepScanningDialogStatus {
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

  // TestObserver should be implemented by tests that need to track when certain
  // ContentAnalysisDialog functions are called. The test can add itself as an
  // observer by using SetObserverForTesting.
  class TestObserver {
   public:
    virtual ~TestObserver() {}

    // Called at the start of ContentAnalysisDialog's constructor. |dialog| is
    // a pointer to the newly constructed ContentAnalysisDialog and should be
    // kept in memory by the test in order to validate its state.
    virtual void ConstructorCalled(ContentAnalysisDialog* dialog,
                                   base::TimeTicks timestamp) {}

    // Called at the end of ContentAnalysisDialog::Show. |timestamp| is the
    // time used by ContentAnalysisDialog to decide whether the pending state
    // has been shown for long enough. The test can keep this time in memory and
    // validate the pending time was sufficient in DialogUpdated.
    virtual void ViewsFirstShown(ContentAnalysisDialog* dialog,
                                 base::TimeTicks timestamp) {}

    // Called at the end of ContentAnalysisDialog::UpdateDialog. |result| is
    // the value that UpdatedDialog used to transition from the pending state to
    // the success/failure/warning state.
    virtual void DialogUpdated(ContentAnalysisDialog* dialog,
                               ContentAnalysisDelegate::FinalResult result) {}

    // Called at the end of ContentAnalysisDialog's destructor. |dialog| is a
    // pointer to the ContentAnalysisDialog being destructed. It can be used
    // to compare it to the pointer obtained from ConstructorCalled to ensure
    // which view is being destroyed.
    virtual void DestructorCalled(ContentAnalysisDialog* dialog) {}
  };

  static void SetObserverForTesting(TestObserver* observer);

  static void SetMinimumPendingDialogTimeForTesting(base::TimeDelta delta);
  static void SetSuccessDialogTimeoutForTesting(base::TimeDelta delta);

  static base::TimeDelta GetMinimumPendingDialogTime();
  static base::TimeDelta GetSuccessDialogTimeout();

  ContentAnalysisDialog(std::unique_ptr<ContentAnalysisDelegate> delegate,
                        content::WebContents* web_contents,
                        safe_browsing::DeepScanAccessPoint access_point,
                        int files_count);

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  ui::ModalType GetModalType() const override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Updates the dialog with the result, and simply delete it from memory if
  // nothing should be shown.
  void ShowResult(ContentAnalysisDelegate::FinalResult result,
                  const std::u16string& custom_message,
                  const GURL& learn_more_url);

  // Accessors to simplify |dialog_status_| checking.
  inline bool is_success() const {
    return dialog_status_ == DeepScanningDialogStatus::SUCCESS;
  }

  inline bool is_failure() const {
    return dialog_status_ == DeepScanningDialogStatus::FAILURE;
  }

  inline bool is_warning() const {
    return dialog_status_ == DeepScanningDialogStatus::WARNING;
  }

  inline bool is_result() const { return !is_pending(); }

  inline bool is_pending() const {
    return dialog_status_ == DeepScanningDialogStatus::PENDING;
  }

  bool has_custom_message() const { return !final_custom_message_.empty(); }

  bool has_learn_more_url() const {
    return !final_learn_more_url_.is_empty() &&
           final_learn_more_url_.is_valid();
  }

  // Returns the side image's logo color depending on |dialog_status_|.
  SkColor GetSideImageLogoColor() const;

  // Returns the side image's background circle color depending on
  // |dialog_status_|.
  SkColor GetSideImageBackgroundColor() const;

  // Returns the appropriate top image depending on |dialog_status_|.
  const gfx::ImageSkia* GetTopImage() const;

  // Accessors used to validate the views in tests.
  views::ImageView* GetTopImageForTesting() const;
  views::Throbber* GetSideIconSpinnerForTesting() const;
  views::Label* GetMessageForTesting() const;

 private:
  ~ContentAnalysisDialog() override;

  // Update the UI depending on |dialog_status_|.
  void UpdateDialog();

  // Resizes the already shown dialog to accommodate changes in its content.
  void Resize(int height_to_add);

  // Setup the appropriate buttons depending on |dialog_status_|.
  void SetupButtons();

  // Returns a newly created side icon.
  std::unique_ptr<views::View> CreateSideIcon();

  // Returns the appropriate dialog message depending on |dialog_status_|.
  std::u16string GetDialogMessage() const;

  // Returns the text for the Cancel button depending on |dialog_status_|.
  std::u16string GetCancelButtonText() const;

  // Returns the text for the Ok button for the warning case.
  std::u16string GetBypassWarningButtonText() const;

  // Returns the appropriate paste top image ID depending on |dialog_status_|.
  int GetPasteImageId(bool use_dark) const;

  // Returns the appropriate upload top image ID depending on |dialog_status_|.
  int GetUploadImageId(bool use_dark) const;

  // Returns the appropriate pending message depending on |files_count_|.
  std::u16string GetPendingMessage() const;

  // Returns the appropriate failure message depending on |final_result_| and
  // |files_count_|.
  std::u16string GetFailureMessage() const;

  // Returns the appropriate warning message depending on |files_count_|.
  std::u16string GetWarningMessage() const;

  // Returns the appropriate success message depending on |files_count_|.
  std::u16string GetSuccessMessage() const;

  std::u16string GetCustomMessage() const;

  // Show the dialog. Sets |shown_| to true.
  void Show();

  void AcceptButtonCallback();
  void CancelButtonCallback();
  void LearnMoreLinkClickedCallback(const ui::Event& event);

  // This callback used by DialogDelegate::SetCancelCallback and is used to
  // ensure the auto-closing success dialog handles focus correctly.
  void SuccessCallback();

  std::unique_ptr<ContentAnalysisDelegate> delegate_;

  content::WebContents* web_contents_;

  // Views above the buttons. |contents_view_| owns every other view.
  views::View* contents_view_ = nullptr;
  DeepScanningTopImageView* image_ = nullptr;
  DeepScanningSideIconImageView* side_icon_image_ = nullptr;
  DeepScanningSideIconSpinnerView* side_icon_spinner_ = nullptr;
  DeepScanningMessageView* message_ = nullptr;
  views::Link* learn_more_link_ = nullptr;

  bool shown_ = false;

  base::TimeTicks first_shown_timestamp_;

  // Used to show the appropriate dialog depending on the scan's status.
  DeepScanningDialogStatus dialog_status_ = DeepScanningDialogStatus::PENDING;

  // Used to show the appropriate message.
  ContentAnalysisDelegate::FinalResult final_result_ =
      ContentAnalysisDelegate::FinalResult::SUCCESS;
  std::u16string final_custom_message_;
  GURL final_learn_more_url_;

  // Used to animate dialog height changes.
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  // The access point that caused this dialog to open. This changes what text
  // and top image are shown to the user.
  safe_browsing::DeepScanAccessPoint access_point_;

  // Indicates whether the scan being done is for files (files_count_>0) or for
  // text (files_count_==0). This changes what text and top image are shown to
  // the user.
  int files_count_;

  base::WeakPtrFactory<ContentAnalysisDialog> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_H_
