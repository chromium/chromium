// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/content_analysis_delegate_base.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class BoundsAnimator;
class BoxLayoutView;
class Link;
class StyledLabel;
class TableLayoutView;
class Textarea;
}  // namespace views

namespace enterprise_connectors {

// Implementation of `views::DialogDelegate` used to show a user the state of
// content analysis triggered by one of their action.
class ContentAnalysisDialogDelegate : public views::DialogDelegate,
                                      public views::TextfieldController,
                                      public ContentAnalysisBaseView::Delegate {
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

    // The dialog is shown with a message indicating that the user has the
    // option to force save the file to cloud storage.
    FORCE_SAVE_TO_CLOUD,
  };

  ContentAnalysisDialogDelegate(
      ContentAnalysisDelegateBase* delegate,
      content::WebContents::Getter web_contents_getter,
      bool is_cloud,
      DeepScanAccessPoint access_point,
      int files_count,
      FinalContentAnalysisResult final_result);
  ~ContentAnalysisDialogDelegate() override;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  ui::mojom::ModalType GetModalType() const override;
  views::View* GetContentsView() override;

  // ContentAnalysisBaseView::Delegate:
  int GetTopImageId() const override;
  ui::ColorId GetSideImageLogoColor() const override;
  ui::ColorId GetSideImageBackgroundColor() const override;
  bool is_result() const override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // Accessors to simplify `dialog_state_` checking.
  inline bool is_success() const { return dialog_state_ == State::SUCCESS; }
  inline bool is_failure() const { return dialog_state_ == State::FAILURE; }
  inline bool is_warning() const { return dialog_state_ == State::WARNING; }
  inline bool is_pending() const { return dialog_state_ == State::PENDING; }
  inline bool is_force_save_to_cloud() const {
    return dialog_state_ == State::FORCE_SAVE_TO_CLOUD;
  }

  // Updates `final_result_` and `dialog_state_`.
  void UpdateStateFromFinalResult(FinalContentAnalysisResult final_result);

  // Update the appearance of the dialog. This will not do anything unless the
  // dialog's state was changed by `UpdateStateFromFinalResult()` since the last
  // `UpdateDialogAppearance()` call.
  void UpdateDialogAppearance();

  // Resets internal members to avoid dangling pointers. Only call this when the
  // owning widget is about to be destroyed.
  void Shutdown();

  // Returns the text entered by the user to justify bypassing a warning, or
  // null if no bypass justification text field was shown.
  std::optional<std::u16string> GetJustification();

  bool has_learn_more_url() const;
  bool bypass_requires_justification() const;
  bool is_cloud() const;
  FinalContentAnalysisResult final_result() const;

  base::WeakPtr<ContentAnalysisDialogDelegate> GetWeakPtr();

  // Accessors used to validate the views in tests.
  views::ImageView* GetTopImageForTesting() const;
  views::Throbber* GetSideIconSpinnerForTesting() const;
  views::StyledLabel* GetMessageForTesting() const;
  views::Link* GetLearnMoreLinkForTesting() const;
  views::Label* GetBypassJustificationLabelForTesting() const;
  views::Textarea* GetBypassJustificationTextareaForTesting() const;
  views::Label* GetJustificationTextLengthForTesting() const;

 private:
  // Helper functions to set/get various parts of the dialog depending on the
  // values of `dialog_state_` and `delegate_base_`.
  void SetupButtons();
  std::u16string GetCancelButtonText() const;
  std::u16string GetDialogMessage() const;
  std::u16string GetPendingMessage() const;
  std::u16string GetFailureMessage() const;
  std::u16string GetWarningMessage() const;
  std::u16string GetSuccessMessage() const;
  std::u16string GetCustomMessage() const;
  std::u16string GetForceSaveToCloudMessage() const;

  bool is_print_scan() const;
  bool has_custom_message() const;
  bool has_custom_message_ranges() const;

  // Updates the views in the dialog to put them in the correct state for
  // `dialog_state_`. This doesn't trigger the same events/resizes as
  // UpdateDialog(), and doesn't require the presence of a widget. This is safe
  // to use in the first GetContentsView() call, before the dialog is shown.
  void UpdateViews();

  // Resizes the already shown dialog to accommodate changes in its content.
  void Resize(int height_to_add);

  // Helper methods to get the admin message shown in dialog.
  void AddLinksToDialogMessage();
  void UpdateDialogMessage(std::u16string new_message);

  // Helper methods to add views to `contents_view_` and `contents_layout_` that
  // are not used for every state of the dialog.
  void AddLearnMoreLinkToDialog();
  void AddJustificationTextLabelToDialog();
  void AddJustificationTextAreaToDialog();
  void AddJustificationTextLengthToDialog();

  void LearnMoreLinkClickedCallback(const ui::Event& event);

  // Returns a newly created side icon. The created views are set to
  // `side_icon_image_` and `side_icon_spinner_`.
  std::unique_ptr<views::View> CreateSideIcon();

  // Views above the buttons. `contents_view_` owns every other view.
  raw_ptr<views::BoxLayoutView> contents_view_ = nullptr;
  raw_ptr<ContentAnalysisTopImageView> image_ = nullptr;
  raw_ptr<ContentAnalysisSideIconImageView> side_icon_image_ = nullptr;
  raw_ptr<ContentAnalysisSideIconSpinnerView> side_icon_spinner_ = nullptr;
  raw_ptr<views::StyledLabel> message_ = nullptr;

  // The following views are also owned by `contents_view_`, but remain nullptr
  // if they aren't required to be initialized.
  raw_ptr<views::Link> learn_more_link_ = nullptr;
  raw_ptr<views::Label> justification_text_label_ = nullptr;
  raw_ptr<views::Textarea> bypass_justification_ = nullptr;
  raw_ptr<views::Label> bypass_justification_text_length_ = nullptr;

  // Table layout owned by `contents_view_`.
  raw_ptr<views::TableLayoutView> contents_layout_ = nullptr;

  // Used to animate dialog height changes.
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  // Used to show the appropriate message.
  FinalContentAnalysisResult final_result_;

  // Used to show the appropriate dialog depending on the scan's status.
  State dialog_state_ = State::PENDING;

  // Should be owned by the parent of this class.
  raw_ptr<ContentAnalysisDelegateBase> delegate_base_;

  content::WebContents::Getter web_contents_getter_;

  // True when performing a cloud-based content analysis, false when performing
  // a locally based content analysis.
  bool is_cloud_ = true;

  // The access point that caused this dialog to open. This changes what text
  // and top image are shown to the user.
  DeepScanAccessPoint access_point_;

  // Indicates whether the scan being done is for files (files_count_>0) or for
  // text (files_count_==0). This changes what text and top image are shown to
  // the user.
  int files_count_;

  base::WeakPtrFactory<ContentAnalysisDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_DELEGATE_H_
