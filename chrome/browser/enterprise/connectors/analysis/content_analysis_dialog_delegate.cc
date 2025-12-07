// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_delegate.h"

#include <string>

#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout_view.h"

namespace enterprise_connectors {

namespace {

constexpr base::TimeDelta kResizeAnimationDuration = base::Milliseconds(100);

constexpr gfx::Insets kSideImageInsets(8);

constexpr int kLineHeight = 20;
constexpr int kSideImageSize = 24;
constexpr int kPaddingBeforeBypassJustification = 16;
constexpr size_t kMaxBypassJustificationLength = 280;
constexpr int kMessageAndIconRowLeadingPadding = 32;
constexpr int kMessageAndIconRowTrailingPadding = 48;
constexpr int kSideIconBetweenChildSpacing = 16;

}  // namespace

ContentAnalysisDialogDelegate::ContentAnalysisDialogDelegate(
    ContentAnalysisDelegateBase* delegate,
    content::WebContents::Getter web_contents_getter,
    bool is_cloud,
    DeepScanAccessPoint access_point,
    int files_count,
    FinalContentAnalysisResult final_result)
    : final_result_(final_result),
      delegate_base_(delegate),
      web_contents_getter_(web_contents_getter),
      is_cloud_(is_cloud),
      access_point_(access_point),
      files_count_(files_count) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetupButtons();

  if (is_warning() && bypass_requires_justification()) {
    bypass_justification_text_length_->SetEnabledColor(
        bypass_justification_text_length_->GetColorProvider()->GetColor(
            ui::kColorAlertHighSeverity));
  }
}

ContentAnalysisDialogDelegate::~ContentAnalysisDialogDelegate() {
  if (bypass_justification_) {
    bypass_justification_->SetController(nullptr);
  }
}

std::u16string ContentAnalysisDialogDelegate::GetWindowTitle() const {
  return std::u16string();
}

bool ContentAnalysisDialogDelegate::ShouldShowCloseButton() const {
  return false;
}

ui::mojom::ModalType ContentAnalysisDialogDelegate::GetModalType() const {
  return ui::mojom::ModalType::kChild;
}

views::View* ContentAnalysisDialogDelegate::GetContentsView() {
  if (!contents_view_) {
    DVLOG(1) << __func__ << ": first time";
    contents_view_ = new views::BoxLayoutView();  // Owned by caller.
    contents_view_->SetOrientation(views::BoxLayout::Orientation::kVertical);
    // Padding to distance the top image from the icon and message.
    contents_view_->SetBetweenChildSpacing(16);

    // padding to distance the message from the button(s).  When doing a cloud
    // based analysis, a top image is added to the view and the top padding
    // looks fine.  When not doing a cloud-based analysis set the top padding
    // to make things look nice.
    contents_view_->SetInsideBorderInsets(
        gfx::Insets::TLBR(is_cloud_ ? 0 : 24, 0, 10, 0));

    // Add the top image for cloud-based analysis.
    if (is_cloud_) {
      image_ = contents_view_->AddChildView(
          std::make_unique<ContentAnalysisTopImageView>(this));
    }

    // Create message area layout.
    contents_layout_ = contents_view_->AddChildView(
        std::make_unique<views::TableLayoutView>());
    contents_layout_
        ->AddPaddingColumn(views::TableLayout::kFixedSize,
                           kMessageAndIconRowLeadingPadding)
        .AddColumn(views::LayoutAlignment::kStart,
                   views::LayoutAlignment::kStart,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          kSideIconBetweenChildSpacing)
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kStretch, 1.0f,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          kMessageAndIconRowTrailingPadding)
        // There is initially only 1 row in the table for the side icon and
        // message. Rows are added later when other elements are needed.
        .AddRows(1, views::TableLayout::kFixedSize);

    // Add the side icon.
    contents_layout_->AddChildView(CreateSideIcon());

    // Add the message.
    message_ =
        contents_layout_->AddChildView(std::make_unique<views::StyledLabel>());
    message_->SetText(GetDialogMessage());
    message_->SetLineHeight(kLineHeight);

    // Calculate the width of the side icon column with insets and padding.
    int side_icon_column_width = kMessageAndIconRowLeadingPadding +
                                 kSideImageInsets.width() + kSideImageSize +
                                 kSideIconBetweenChildSpacing;
    message_->SizeToFit(fixed_width() - side_icon_column_width -
                        kMessageAndIconRowTrailingPadding);
    message_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    if (!is_pending()) {
      UpdateDialogAppearance();
    }
  }

  return contents_view_;
}

int ContentAnalysisDialogDelegate::GetTopImageId() const {
  if (color_utils::IsDark(contents_view_->GetColorProvider()->GetColor(
          ui::kColorDialogBackground))) {
    switch (dialog_state_) {
      case State::PENDING:
        return IDR_UPLOAD_SCANNING_DARK;
      case State::SUCCESS:
        return IDR_UPLOAD_SUCCESS_DARK;
      case State::FAILURE:
      case State::FORCE_SAVE_TO_CLOUD:
        return IDR_UPLOAD_VIOLATION_DARK;
      case State::WARNING:
        return IDR_UPLOAD_WARNING_DARK;
    }
  } else {
    switch (dialog_state_) {
      case State::PENDING:
        return IDR_UPLOAD_SCANNING;
      case State::SUCCESS:
        return IDR_UPLOAD_SUCCESS;
      case State::FAILURE:
      case State::FORCE_SAVE_TO_CLOUD:
        return IDR_UPLOAD_VIOLATION;
      case State::WARNING:
        return IDR_UPLOAD_WARNING;
    }
  }
}

ui::ColorId ContentAnalysisDialogDelegate::GetSideImageLogoColor() const {
  DCHECK(contents_view_);

  switch (dialog_state_) {
    case State::PENDING:
      // In the dialog's pending state, the side image is just an enterprise
      // logo surrounded by a throbber, so we use the throbber color for it.
      return ui::kColorThrobber;
    case State::SUCCESS:
    case State::FAILURE:
    case State::WARNING:
    case State::FORCE_SAVE_TO_CLOUD:
      // In a result state, the side image is a circle colored with the result's
      // color and an enterprise logo in front of it, so the logo should have
      // the same color as the dialog's overall background.
      return ui::kColorDialogBackground;
  }
}

ui::ColorId ContentAnalysisDialogDelegate::GetSideImageBackgroundColor() const {
  DCHECK(is_result());
  DCHECK(contents_view_);

  switch (dialog_state_) {
    case State::PENDING:
      NOTREACHED();
    case State::SUCCESS:
      return ui::kColorAccent;
    case State::FAILURE:
    case State::FORCE_SAVE_TO_CLOUD:
      return ui::kColorAlertHighSeverity;
    case State::WARNING:
      return ui::kColorAlertMediumSeverityIcon;
  }
}

bool ContentAnalysisDialogDelegate::is_result() const {
  return !is_pending();
}

void ContentAnalysisDialogDelegate::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  if (bypass_justification_text_length_) {
    bypass_justification_text_length_->SetText(l10n_util::GetStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_BYPASS_JUSTIFICATION_TEXT_LIMIT_LABEL,
        base::NumberToString16(new_contents.size()),
        base::NumberToString16(kMaxBypassJustificationLength)));
  }

  if (new_contents.size() == 0 ||
      new_contents.size() > kMaxBypassJustificationLength) {
    DialogDelegate::SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
    if (bypass_justification_text_length_) {
      bypass_justification_text_length_->SetEnabledColor(
          bypass_justification_text_length_->GetColorProvider()->GetColor(
              ui::kColorAlertHighSeverity));
    }
  } else {
    DialogDelegate::SetButtonEnabled(ui::mojom::DialogButton::kOk, true);
    if (bypass_justification_text_length_ && justification_text_label_) {
      bypass_justification_text_length_->SetEnabledColor(
          justification_text_label_->GetEnabledColor());
    }
  }
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
    case FinalContentAnalysisResult::FORCE_SAVE_TO_CLOUD:
      dialog_state_ = State::FORCE_SAVE_TO_CLOUD;
      break;
  }
}

void ContentAnalysisDialogDelegate::UpdateDialogAppearance() {
  DCHECK(is_result());

  int height_before = contents_view_->GetPreferredSize().height();

  UpdateViews();

  // Resize the dialog's height. This is needed since the text might take more
  // lines after changing.
  int height_after = contents_view_->GetHeightForWidth(contents_view_->width());

  int height_to_add = std::max(height_after - height_before, 0);
  if (height_to_add > 0 && GetWidget()) {
    Resize(height_to_add);
  }

  // Update the dialog.
  DialogDelegate::DialogModelChanged();
  contents_view_->InvalidateLayout();
}

void ContentAnalysisDialogDelegate::Shutdown() {
  contents_view_ = nullptr;
  image_ = nullptr;
  side_icon_image_ = nullptr;
  side_icon_spinner_ = nullptr;
  message_ = nullptr;
  learn_more_link_ = nullptr;
  justification_text_label_ = nullptr;
  bypass_justification_ = nullptr;
  bypass_justification_text_length_ = nullptr;
  contents_layout_ = nullptr;
  bounds_animator_.reset();
  delegate_base_ = nullptr;
}

std::optional<std::u16string>
ContentAnalysisDialogDelegate::GetJustification() {
  if (delegate_base_->BypassRequiresJustification() && bypass_justification_) {
    return std::u16string(bypass_justification_->GetText());
  }
  return std::nullopt;
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
  } else if (is_force_save_to_cloud()) {
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kCancel) |
        static_cast<int>(ui::mojom::DialogButton::kOk));
    DialogDelegate::SetDefaultButton(
        static_cast<int>(ui::mojom::DialogButton::kCancel));

    // Do not allow overrides, since this option only applies to downloads.
    SetButtonLabel(ui::mojom::DialogButton::kCancel,
                   l10n_util::GetStringUTF16(
                       IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_DISCARD_FILE_BUTTON));

    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(
                       IDS_DEEP_SCANNING_DIALOG_SAVE_TO_CLOUD_STORAGE_LABEL));
  } else {
    // Include no buttons otherwise.
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kNone));
  }
}

std::u16string ContentAnalysisDialogDelegate::GetCancelButtonText() const {
  auto overriden_text = delegate_base_->OverrideCancelButtonText();
  if (overriden_text) {
    return overriden_text.value();
  }

  int text_id;
  switch (dialog_state_) {
    case State::SUCCESS:
      NOTREACHED();
    case State::PENDING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_UPLOAD_BUTTON;
      break;
    case State::FORCE_SAVE_TO_CLOUD:
    case State::FAILURE:
      text_id = IDS_CLOSE;
      break;
    case State::WARNING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_WARNING_BUTTON;
      break;
  }
  return l10n_util::GetStringUTF16(text_id);
}

std::u16string ContentAnalysisDialogDelegate::GetDialogMessage() const {
  switch (dialog_state_) {
    case State::PENDING:
      return GetPendingMessage();
    case State::FORCE_SAVE_TO_CLOUD:
      return GetForceSaveToCloudMessage();
    case State::FAILURE:
      return GetFailureMessage();
    case State::SUCCESS:
      return GetSuccessMessage();
    case State::WARNING:
      return GetWarningMessage();
  }
}

std::u16string ContentAnalysisDialogDelegate::GetPendingMessage() const {
  DCHECK(is_pending());
  if (is_print_scan()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_PRINT_PENDING_MESSAGE);
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialogDelegate::GetForceSaveToCloudMessage()
    const {
  DCHECK(is_force_save_to_cloud());

  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_SAVE_TO_CLOUD_STORAGE_MESSAGE);
}

std::u16string ContentAnalysisDialogDelegate::GetFailureMessage() const {
  DCHECK(is_failure());

  // If the admin has specified a custom message for this failure, it takes
  // precedence over the generic ones.
  if (has_custom_message()) {
    return GetCustomMessage();
  }

  if (final_result_ == FinalContentAnalysisResult::FAIL_CLOSED) {
    DVLOG(1) << __func__ << ": display fail-closed message.";
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAIL_CLOSED_MESSAGE);
  }

  if (final_result_ == FinalContentAnalysisResult::LARGE_FILES) {
    if (is_print_scan()) {
      return l10n_util::GetStringUTF16(
          IDS_DEEP_SCANNING_DIALOG_LARGE_PRINT_FAILURE_MESSAGE);
    }
    return l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_LARGE_FILE_FAILURE_MESSAGE, files_count_);
  }

  if (final_result_ == FinalContentAnalysisResult::ENCRYPTED_FILES) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_ENCRYPTED_FILE_FAILURE_MESSAGE, files_count_);
  }

  if (is_print_scan()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_PRINT_WARNING_MESSAGE);
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialogDelegate::GetWarningMessage() const {
  DCHECK(is_warning());

  // If the admin has specified a custom message for this warning, it takes
  // precedence over the generic one.
  if (has_custom_message()) {
    return GetCustomMessage();
  }

  if (is_print_scan()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_PRINT_WARNING_MESSAGE);
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_WARNING_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialogDelegate::GetSuccessMessage() const {
  DCHECK(is_success());
  if (is_print_scan()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_PRINT_SUCCESS_MESSAGE);
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialogDelegate::GetCustomMessage() const {
  DCHECK(is_warning() || is_failure());
  DCHECK(has_custom_message());
  return *(delegate_base_->GetCustomMessage());
}

bool ContentAnalysisDialogDelegate::is_print_scan() const {
  return access_point_ == DeepScanAccessPoint::PRINT;
}

bool ContentAnalysisDialogDelegate::has_custom_message() const {
  return delegate_base_->GetCustomMessage().has_value();
}

bool ContentAnalysisDialogDelegate::has_learn_more_url() const {
  return delegate_base_->GetCustomLearnMoreUrl().has_value();
}

bool ContentAnalysisDialogDelegate::has_custom_message_ranges() const {
  return delegate_base_->GetCustomRuleMessageRanges().has_value();
}

bool ContentAnalysisDialogDelegate::bypass_requires_justification() const {
  return delegate_base_->BypassRequiresJustification();
}

bool ContentAnalysisDialogDelegate::is_cloud() const {
  return is_cloud_;
}

FinalContentAnalysisResult ContentAnalysisDialogDelegate::final_result() const {
  return final_result_;
}

base::WeakPtr<ContentAnalysisDialogDelegate>
ContentAnalysisDialogDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

views::ImageView* ContentAnalysisDialogDelegate::GetTopImageForTesting() const {
  return image_;
}

views::Throbber* ContentAnalysisDialogDelegate::GetSideIconSpinnerForTesting()
    const {
  return side_icon_spinner_;
}

views::StyledLabel* ContentAnalysisDialogDelegate::GetMessageForTesting()
    const {
  return message_;
}

views::Link* ContentAnalysisDialogDelegate::GetLearnMoreLinkForTesting() const {
  return learn_more_link_;
}

views::Label*
ContentAnalysisDialogDelegate::GetBypassJustificationLabelForTesting() const {
  return justification_text_label_;
}

views::Textarea*
ContentAnalysisDialogDelegate::GetBypassJustificationTextareaForTesting()
    const {
  return bypass_justification_;
}

views::Label*
ContentAnalysisDialogDelegate::GetJustificationTextLengthForTesting() const {
  return bypass_justification_text_length_;
}

void ContentAnalysisDialogDelegate::UpdateViews() {
  DCHECK(contents_view_);

  // Update the style of the dialog to reflect the new state.
  if (image_) {
    image_->Update();
  }

  side_icon_image_->Update();

  // There isn't always a spinner, for instance when the dialog is started in a
  // state other than the "pending" state.
  if (side_icon_spinner_) {
    // Calling `Update` leads to the deletion of the spinner.
    side_icon_spinner_.ExtractAsDangling()->Update();
  }

  // Update the buttons.
  SetupButtons();

  // Update the message's text, and send an alert for screen readers since the
  // text changed.
  UpdateDialogMessage(GetDialogMessage());

  // Add bypass justification views when required on warning verdicts. The order
  // of the helper functions needs to be preserved for them to appear in the
  // correct order.
  if (is_warning() && bypass_requires_justification()) {
    AddJustificationTextLabelToDialog();
    AddJustificationTextAreaToDialog();
    AddJustificationTextLengthToDialog();
  }
}

void ContentAnalysisDialogDelegate::Resize(int height_to_add) {
  // Only resize if the dialog is updated to show a result.
  DCHECK(is_result());
  views::Widget* widget = GetWidget();
  DCHECK(widget);

  gfx::Rect dialog_rect = widget->GetContentsView()->GetContentsBounds();
  int new_height = dialog_rect.height();

  // Remove the button row's height if it's removed in the success case.
  if (is_success()) {
    DCHECK(contents_view_->parent());
    DCHECK_EQ(contents_view_->parent()->children().size(), 2ul);
    DCHECK_EQ(contents_view_->parent()->children()[0], contents_view_);

    views::View* button_row_view = contents_view_->parent()->children()[1];
    new_height -= button_row_view->GetContentsBounds().height();
  }

  // Apply the message lines delta.
  new_height += height_to_add;
  dialog_rect.set_height(new_height);

  // Setup the animation.
  bounds_animator_ =
      std::make_unique<views::BoundsAnimator>(widget->GetRootView());
  bounds_animator_->SetAnimationDuration(kResizeAnimationDuration);

  DCHECK(widget->GetRootView());
  views::View* view_to_resize = widget->GetRootView()->children()[0];

  // Start the animation.
  bounds_animator_->AnimateViewTo(view_to_resize, dialog_rect);

  // Change the widget's size.
  gfx::Size new_size = view_to_resize->size();
  new_size.set_height(new_height);
  widget->SetSize(new_size);
}

void ContentAnalysisDialogDelegate::AddLinksToDialogMessage() {
  if (!has_custom_message_ranges()) {
    return;
  }

  std::vector<std::pair<gfx::Range, GURL>> ranges =
      *(delegate_base_->GetCustomRuleMessageRanges());
  for (const auto& range : ranges) {
    if (!range.second.is_valid()) {
      continue;
    }
    message_->AddStyleRange(
        gfx::Range(range.first.start(), range.first.end()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            [](base::WeakPtr<content::WebContents> web_contents, GURL url,
               const ui::Event& event) {
              if (!web_contents) {
                return;
              }
              web_contents->OpenURL(
                  content::OpenURLParams(
                      url, content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_LINK,
                      /*is_renderer_initiated=*/false),
                  /*navigation_handle_callback=*/{});
            },
            web_contents_getter_.Run()->GetWeakPtr(), range.second)));
  }
}

void ContentAnalysisDialogDelegate::UpdateDialogMessage(
    std::u16string new_message) {
  if ((is_failure() || is_warning()) && has_custom_message()) {
    message_->SetText(new_message);
    AddLinksToDialogMessage();
    message_->GetViewAccessibility().AnnounceText(std::move(new_message));
    if (has_learn_more_url()) {
      AddLearnMoreLinkToDialog();
    }
  } else {
    message_->SetText(new_message);
    message_->GetViewAccessibility().AnnounceText(std::move(new_message));

    // Add a "Learn More" link for warnings/failures when one is provided.
    if ((is_failure() || is_warning()) && has_learn_more_url()) {
      AddLearnMoreLinkToDialog();
    }
  }
}

void ContentAnalysisDialogDelegate::AddLearnMoreLinkToDialog() {
  DCHECK(contents_view_);
  DCHECK(contents_layout_);
  DCHECK(is_warning() || is_failure());

  // There is only ever up to one link in the dialog, so return early instead of
  // adding another one.
  if (learn_more_link_) {
    return;
  }

  // Add a row for the new element, and add an empty view to skip the first
  // column.
  contents_layout_->AddRows(1, views::TableLayout::kFixedSize);
  contents_layout_->AddChildView(std::make_unique<views::View>());

  // Since `learn_more_link_` is not as wide as the column it's a part of,
  // instead of being added directly to it, it has a parent with a BoxLayout so
  // that its width corresponds to its own text size instead of the full column
  // width.
  views::View* learn_more_column =
      contents_layout_->AddChildView(std::make_unique<views::View>());
  learn_more_column->SetLayoutManager(std::make_unique<views::BoxLayout>());

  learn_more_link_ = learn_more_column->AddChildView(
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(
          IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE_LEARN_MORE_LINK)));
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  learn_more_link_->SetCallback(base::BindRepeating(
      &ContentAnalysisDialogDelegate::LearnMoreLinkClickedCallback,
      base::Unretained(this)));
}

void ContentAnalysisDialogDelegate::AddJustificationTextLabelToDialog() {
  DCHECK(contents_view_);
  DCHECK(contents_layout_);
  DCHECK(is_warning());

  // There is only ever up to one justification section in the dialog, so return
  // early instead of adding another one.
  if (justification_text_label_) {
    return;
  }

  // Add a row for the new element, and add an empty view to skip the first
  // column.
  contents_layout_->AddRows(1, views::TableLayout::kFixedSize);
  contents_layout_->AddChildView(std::make_unique<views::View>());

  justification_text_label_ =
      contents_layout_->AddChildView(std::make_unique<views::Label>());
  justification_text_label_->SetText(
      delegate_base_->GetBypassJustificationLabel());
  justification_text_label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kPaddingBeforeBypassJustification, 0, 0, 0)));
  justification_text_label_->SetLineHeight(kLineHeight);
  justification_text_label_->SetMultiLine(true);
  justification_text_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  justification_text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void ContentAnalysisDialogDelegate::AddJustificationTextAreaToDialog() {
  DCHECK(contents_view_);
  DCHECK(contents_layout_);
  DCHECK(justification_text_label_);
  DCHECK(is_warning());

  // There is only ever up to one justification text box in the dialog, so
  // return early instead of adding another one.
  if (bypass_justification_) {
    return;
  }

  // Add a row for the new element, and add an empty view to skip the first
  // column.
  contents_layout_->AddRows(1, views::TableLayout::kFixedSize);
  contents_layout_->AddChildView(std::make_unique<views::View>());

  bypass_justification_ =
      contents_layout_->AddChildView(std::make_unique<views::Textarea>());
  bypass_justification_->GetViewAccessibility().SetName(
      *justification_text_label_);
  bypass_justification_->SetController(this);
}

void ContentAnalysisDialogDelegate::AddJustificationTextLengthToDialog() {
  DCHECK(contents_view_);
  DCHECK(contents_layout_);
  DCHECK(is_warning());

  // There is only ever up to one justification text length indicator in the
  // dialog, so return early instead of adding another one.
  if (bypass_justification_text_length_) {
    return;
  }

  // Add a row for the new element, and add an empty view to skip the first
  // column.
  contents_layout_->AddRows(1, views::TableLayout::kFixedSize);
  contents_layout_->AddChildView(std::make_unique<views::View>());

  bypass_justification_text_length_ =
      contents_layout_->AddChildView(std::make_unique<views::Label>());
  bypass_justification_text_length_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  bypass_justification_text_length_->SetText(l10n_util::GetStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_BYPASS_JUSTIFICATION_TEXT_LIMIT_LABEL,
      base::NumberToString16(0),
      base::NumberToString16(kMaxBypassJustificationLength)));

  // Set the color to red initially because a 0 length message is invalid. Skip
  // this if the color provider is unavailable.
  if (bypass_justification_text_length_->GetColorProvider()) {
    bypass_justification_text_length_->SetEnabledColor(
        bypass_justification_text_length_->GetColorProvider()->GetColor(
            ui::kColorAlertHighSeverity));
  }
}

void ContentAnalysisDialogDelegate::LearnMoreLinkClickedCallback(
    const ui::Event& event) {
  DCHECK(has_learn_more_url());
  web_contents_getter_.Run()->OpenURL(
      content::OpenURLParams((*delegate_base_->GetCustomLearnMoreUrl()),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

std::unique_ptr<views::View> ContentAnalysisDialogDelegate::CreateSideIcon() {
  // The icon left of the text has the appearance of a blue "Enterprise" logo
  // with a spinner when the scan is pending.
  auto icon = std::make_unique<views::View>();
  icon->SetLayoutManager(std::make_unique<views::FillLayout>());
  side_icon_image_ = icon->AddChildView(
      std::make_unique<ContentAnalysisSideIconImageView>(this));

  // Add a spinner if the scan result is pending.
  if (is_pending()) {
    auto spinner = std::make_unique<ContentAnalysisSideIconSpinnerView>(this);
    spinner->Start();
    side_icon_spinner_ = icon->AddChildView(std::move(spinner));
  }

  return icon;
}

}  // namespace enterprise_connectors
