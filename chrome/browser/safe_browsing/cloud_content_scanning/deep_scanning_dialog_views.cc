// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"

#include <memory>

#include "base/bind.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace safe_browsing {

namespace {

constexpr base::TimeDelta kResizeAnimationDuration =
    base::TimeDelta::FromMilliseconds(100);

constexpr int kSideImageSize = 24;
constexpr int kLineHeight = 20;

constexpr gfx::Insets kSideImageInsets = gfx::Insets(8, 8, 8, 8);
constexpr gfx::Insets kMessageAndIconRowInsets = gfx::Insets(0, 32, 0, 48);
constexpr int kSideIconBetweenChildSpacing = 16;

// These time values are non-const in order to be overridden in test so they
// complete faster.
base::TimeDelta minimum_pending_dialog_time_ = base::TimeDelta::FromSeconds(2);
base::TimeDelta success_dialog_timeout_ = base::TimeDelta::FromSeconds(1);

// A simple background class to show a colored circle behind the side icon once
// the scanning is done.
class CircleBackground : public views::Background {
 public:
  explicit CircleBackground(SkColor color) { SetNativeControlColor(color); }
  ~CircleBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    int radius = view->bounds().width() / 2;
    gfx::PointF center(radius, radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawCircle(center, radius, flags);
  }
};

SkColor GetBackgroundColor(const views::Widget* widget) {
  return widget->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
}

DeepScanningDialogViews::TestObserver* observer_for_testing = nullptr;

}  // namespace

// View classes used to override OnThemeChanged and update the sub-views to the
// new theme.

class DeepScanningBaseView {
 public:
  explicit DeepScanningBaseView(DeepScanningDialogViews* dialog)
      : dialog_(dialog) {}
  DeepScanningDialogViews* dialog() { return dialog_; }

 protected:
  DeepScanningDialogViews* dialog_;
};

class DeepScanningTopImageView : public DeepScanningBaseView,
                                 public views::ImageView {
 public:
  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() { SetImage(dialog()->GetTopImage()); }

 protected:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    Update();
  }
};

class DeepScanningSideIconImageView : public DeepScanningBaseView,
                                      public views::ImageView {
 public:
  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() {
    SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon, kSideImageSize,
                                   dialog()->GetSideImageLogoColor()));
    if (dialog()->is_result()) {
      SetBackground(std::make_unique<CircleBackground>(
          dialog()->GetSideImageBackgroundColor()));
    }
  }

 protected:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    Update();
  }
};

class DeepScanningSideIconSpinnerView : public DeepScanningBaseView,
                                        public views::Throbber {
 public:
  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() {
    if (dialog()->is_result()) {
      parent()->RemoveChildView(this);
      delete this;
    }
  }

 protected:
  void OnThemeChanged() override {
    views::Throbber::OnThemeChanged();
    Update();
  }
};

class DeepScanningMessageView : public DeepScanningBaseView,
                                public views::Label {
 public:
  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() {
    if (dialog()->is_failure() || dialog()->is_warning())
      SetEnabledColor(dialog()->GetSideImageBackgroundColor());
  }

 protected:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    Update();
  }
};

// static
base::TimeDelta DeepScanningDialogViews::GetMinimumPendingDialogTime() {
  return minimum_pending_dialog_time_;
}

// static
base::TimeDelta DeepScanningDialogViews::GetSuccessDialogTimeout() {
  return success_dialog_timeout_;
}

DeepScanningDialogViews::DeepScanningDialogViews(
    std::unique_ptr<DeepScanningDialogDelegate> delegate,
    content::WebContents* web_contents,
    DeepScanAccessPoint access_point,
    int files_count)
    : content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      web_contents_(web_contents),
      access_point_(std::move(access_point)),
      files_count_(files_count) {
  SetOwnedByWidget(true);

  if (observer_for_testing)
    observer_for_testing->ConstructorCalled(this, base::TimeTicks::Now());

  Show();
}

base::string16 DeepScanningDialogViews::GetWindowTitle() const {
  return base::string16();
}

void DeepScanningDialogViews::AcceptButtonCallback() {
  DCHECK(delegate_);
  DCHECK(is_warning());
  delegate_->BypassWarnings();
}

void DeepScanningDialogViews::CancelButtonCallback() {
  if (delegate_)
    delegate_->Cancel(is_warning());
}

bool DeepScanningDialogViews::ShouldShowCloseButton() const {
  return false;
}

views::View* DeepScanningDialogViews::GetContentsView() {
  if (!contents_view_) {
    contents_view_ = new views::View();  // Owned by caller.

    // Create layout
    views::GridLayout* layout =
        contents_view_->SetLayoutManager(std::make_unique<views::GridLayout>());
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(
        /*h_align=*/views::GridLayout::FILL,
        /*v_align=*/views::GridLayout::FILL,
        /*resize_percent=*/1.0,
        /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
        /*fixed_width=*/0,
        /*min_width=*/0);

    // Add the top image.
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    image_ = layout->AddView(std::make_unique<DeepScanningTopImageView>(this));

    // Add padding to distance the top image from the icon and message.
    layout->AddPaddingRow(views::GridLayout::kFixedSize, 16);

    // Add the side icon and message row.
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    auto icon_and_message_row = std::make_unique<views::View>();
    auto* row_layout = icon_and_message_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            kMessageAndIconRowInsets, kSideIconBetweenChildSpacing));
    row_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    row_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Add the side icon.
    icon_and_message_row->AddChildView(CreateSideIcon());

    // Add the message.
    auto label = std::make_unique<DeepScanningMessageView>(this);
    label->SetText(GetDialogMessage());
    label->SetLineHeight(kLineHeight);
    label->SetMultiLine(true);
    label->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_ = icon_and_message_row->AddChildView(std::move(label));

    layout->AddView(std::move(icon_and_message_row));

    // Add padding to distance the message from the button(s).
    layout->AddPaddingRow(views::GridLayout::kFixedSize, 10);
  }

  return contents_view_;
}

views::Widget* DeepScanningDialogViews::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* DeepScanningDialogViews::GetWidget() const {
  return contents_view_->GetWidget();
}

ui::ModalType DeepScanningDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void DeepScanningDialogViews::WebContentsDestroyed() {
  // If |web_contents_| is destroyed, then the scan results don't matter so the
  // delegate can be destroyed as well.
  delegate_.reset(nullptr);
  CancelDialog();
}

void DeepScanningDialogViews::ShowResult(
    DeepScanningDialogDelegate::DeepScanningFinalResult result) {
  DCHECK(is_pending());
  final_result_ = result;
  switch (final_result_) {
    case DeepScanningDialogDelegate::DeepScanningFinalResult::ENCRYPTED_FILES:
    case DeepScanningDialogDelegate::DeepScanningFinalResult::LARGE_FILES:
    case DeepScanningDialogDelegate::DeepScanningFinalResult::FAILURE:
      dialog_status_ = DeepScanningDialogStatus::FAILURE;
      break;
    case DeepScanningDialogDelegate::DeepScanningFinalResult::SUCCESS:
      dialog_status_ = DeepScanningDialogStatus::SUCCESS;
      break;
    case DeepScanningDialogDelegate::DeepScanningFinalResult::WARNING:
      dialog_status_ = DeepScanningDialogStatus::WARNING;
      break;
  }

  // Do nothing if the pending dialog wasn't shown, the delayed |Show| callback
  // will show the negative result later if that's the verdict.
  if (!shown_) {
    // Cleanup if the pending dialog wasn't shown and the verdict is safe.
    if (is_success())
      delete this;
    return;
  }

  // Update the pending dialog only after it has been shown for a minimum amount
  // of time.
  base::TimeDelta time_shown = base::TimeTicks::Now() - first_shown_timestamp_;
  if (time_shown >= GetMinimumPendingDialogTime()) {
    UpdateDialog();
  } else {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeepScanningDialogViews::UpdateDialog,
                       weak_ptr_factory_.GetWeakPtr()),
        GetMinimumPendingDialogTime() - time_shown);
  }
}

DeepScanningDialogViews::~DeepScanningDialogViews() {
  if (observer_for_testing)
    observer_for_testing->DestructorCalled(this);
}

void DeepScanningDialogViews::UpdateDialog() {
  DCHECK(shown_);
  views::Widget* widget = GetWidget();
  DCHECK(widget);
  DCHECK(is_result());

  // Update the style of the dialog to reflect the new state.
  message_->Update();
  image_->Update();
  side_icon_image_->Update();
  side_icon_spinner_->Update();
  side_icon_spinner_ = nullptr;

  // Update the buttons.
  SetupButtons();

  // Update the message's text, and send an alert for screen readers since the
  // text changed.
  base::string16 new_message = GetDialogMessage();
  message_->SetText(new_message);
  message_->GetViewAccessibility().AnnounceText(std::move(new_message));

  // Resize the dialog's height. This is needed since the text might take more
  // lines after changing.
  int text_height = message_->GetRequiredLines() * message_->GetLineHeight();
  int row_height = message_->parent()->height();
  int height_to_add = std::max(text_height - row_height, 0);
  if (height_to_add > 0)
    Resize(height_to_add);

  // Update the dialog.
  DialogDelegate::DialogModelChanged();
  widget->ScheduleLayout();

  // Schedule the dialog to close itself in the success case.
  if (is_success()) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DialogDelegate::CancelDialog,
                       weak_ptr_factory_.GetWeakPtr()),
        GetSuccessDialogTimeout());
  }

  if (observer_for_testing)
    observer_for_testing->DialogUpdated(this, final_result_);

  // Cancel the dialog as it is updated in tests in the failure dialog case.
  // This is necessary to terminate tests that end when the dialog is closed.
  if (observer_for_testing && is_failure())
    CancelDialog();
}

void DeepScanningDialogViews::Resize(int height_to_add) {
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
  DCHECK_EQ(widget->GetRootView()->children().size(), 1u);
  views::View* view_to_resize = widget->GetRootView()->children()[0];

  // Start the animation.
  bounds_animator_->AnimateViewTo(view_to_resize, dialog_rect);

  // Change the widget's size.
  gfx::Size new_size = view_to_resize->size();
  new_size.set_height(new_height);
  widget->SetSize(new_size);
}

void DeepScanningDialogViews::SetupButtons() {
  // TODO(domfc): Add "Learn more" button on scan failure.
  if (is_warning()) {
    // Include the Ok and Cancel buttons if there is a bypassable warning.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_CANCEL |
                                ui::DIALOG_BUTTON_OK);
    DialogDelegate::SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);

    DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                                     GetCancelButtonText());
    DialogDelegate::SetCancelCallback(
        base::BindOnce(&DeepScanningDialogViews::CancelButtonCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_OK,
                                     GetBypassWarningButtonText());
    DialogDelegate::SetAcceptCallback(
        base::BindOnce(&DeepScanningDialogViews::AcceptButtonCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (is_failure() || is_pending()) {
    // Include the Cancel button when the scan is pending or failing.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_CANCEL);
    DialogDelegate::SetDefaultButton(ui::DIALOG_BUTTON_NONE);

    DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                                     GetCancelButtonText());
    DialogDelegate::SetCancelCallback(
        base::BindOnce(&DeepScanningDialogViews::CancelButtonCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Include no buttons otherwise.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  }
}

base::string16 DeepScanningDialogViews::GetDialogMessage() const {
  switch (dialog_status_) {
    case DeepScanningDialogStatus::PENDING:
      return GetPendingMessage();
    case DeepScanningDialogStatus::FAILURE:
      return GetFailureMessage();
    case DeepScanningDialogStatus::SUCCESS:
      return GetSuccessMessage();
    case DeepScanningDialogStatus::WARNING:
      return GetWarningMessage();
  }
}

base::string16 DeepScanningDialogViews::GetCancelButtonText() const {
  int text_id;
  switch (dialog_status_) {
    case DeepScanningDialogStatus::SUCCESS:
      NOTREACHED();
      FALLTHROUGH;
    case DeepScanningDialogStatus::PENDING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_UPLOAD_BUTTON;
      break;
    case DeepScanningDialogStatus::FAILURE:
      text_id = IDS_CLOSE;
      break;
    case DeepScanningDialogStatus::WARNING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_WARNING_BUTTON;
      break;
  }
  return l10n_util::GetStringUTF16(text_id);
}

base::string16 DeepScanningDialogViews::GetBypassWarningButtonText() const {
  DCHECK(is_warning());
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_PROCEED_BUTTON);
}

void DeepScanningDialogViews::Show() {
  DCHECK(!shown_);

  // The only state that cannot be shown immediately is SUCCESS, the dialog
  // doesn't appear in that case.
  DCHECK(!is_success());

  shown_ = true;
  first_shown_timestamp_ = base::TimeTicks::Now();

  SetupButtons();

  constrained_window::ShowWebModalDialogViews(this, web_contents_);

  if (observer_for_testing)
    observer_for_testing->ViewsFirstShown(this, first_shown_timestamp_);

  // Cancel the dialog as it is shown in tests if the failure dialog is shown
  // immediately.
  if (observer_for_testing && is_failure())
    CancelDialog();
}

std::unique_ptr<views::View> DeepScanningDialogViews::CreateSideIcon() {
  // The side icon is created either:
  // - When the pending dialog is shown
  // - When the response was fast enough that the failure dialog is shown first
  DCHECK(!is_success());

  // The icon left of the text has the appearance of a blue "Enterprise" logo
  // with a spinner when the scan is pending.
  auto icon = std::make_unique<views::View>();
  icon->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto side_image = std::make_unique<DeepScanningSideIconImageView>(this);
  side_image->SetImage(gfx::CreateVectorIcon(
      gfx::IconDescription(vector_icons::kBusinessIcon, kSideImageSize)));
  side_image->SetBorder(views::CreateEmptyBorder(kSideImageInsets));
  side_icon_image_ = icon->AddChildView(std::move(side_image));

  // Add a spinner if the scan result is pending.
  if (is_pending()) {
    auto spinner = std::make_unique<DeepScanningSideIconSpinnerView>(this);
    spinner->Start();
    side_icon_spinner_ = icon->AddChildView(std::move(spinner));
  }

  return icon;
}

SkColor DeepScanningDialogViews::GetSideImageBackgroundColor() const {
  DCHECK(is_result());
  const views::Widget* widget = GetWidget();
  DCHECK(widget);
  ui::NativeTheme::ColorId color_id =
      is_success() ? ui::NativeTheme::kColorId_AlertSeverityLow
                   : ui::NativeTheme::kColorId_AlertSeverityHigh;
  return widget->GetNativeTheme()->GetSystemColor(color_id);
}

int DeepScanningDialogViews::GetPasteImageId(bool use_dark) const {
  if (is_pending())
    return use_dark ? IDR_PASTE_SCANNING_DARK : IDR_PASTE_SCANNING;
  if (is_success())
    return use_dark ? IDR_PASTE_SUCCESS_DARK : IDR_PASTE_SUCCESS;
  return use_dark ? IDR_PASTE_VIOLATION_DARK : IDR_PASTE_VIOLATION;
}

int DeepScanningDialogViews::GetUploadImageId(bool use_dark) const {
  if (is_pending())
    return use_dark ? IDR_UPLOAD_SCANNING_DARK : IDR_UPLOAD_SCANNING;
  if (is_success())
    return use_dark ? IDR_UPLOAD_SUCCESS_DARK : IDR_UPLOAD_SUCCESS;
  return use_dark ? IDR_UPLOAD_VIOLATION_DARK : IDR_UPLOAD_VIOLATION;
}

base::string16 DeepScanningDialogViews::GetPendingMessage() const {
  DCHECK(is_pending());
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE, files_count_);
}

base::string16 DeepScanningDialogViews::GetFailureMessage() const {
  DCHECK(is_failure());

  if (final_result_ ==
      DeepScanningDialogDelegate::DeepScanningFinalResult::LARGE_FILES) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_LARGE_FILE_FAILURE_MESSAGE, files_count_);
  }

  if (final_result_ ==
      DeepScanningDialogDelegate::DeepScanningFinalResult::ENCRYPTED_FILES) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_ENCRYPTED_FILE_FAILURE_MESSAGE, files_count_);
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE, files_count_);
}

base::string16 DeepScanningDialogViews::GetWarningMessage() const {
  DCHECK(is_warning());
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_WARNING_MESSAGE, files_count_);
}

base::string16 DeepScanningDialogViews::GetSuccessMessage() const {
  DCHECK(is_success());
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE, files_count_);
}

const gfx::ImageSkia* DeepScanningDialogViews::GetTopImage() const {
  const bool use_dark = color_utils::IsDark(GetBackgroundColor(GetWidget()));
  const bool treat_as_text_paste =
      access_point_ == DeepScanAccessPoint::PASTE ||
      (access_point_ == DeepScanAccessPoint::DRAG_AND_DROP &&
       files_count_ == 0);

  int image_id = treat_as_text_paste ? GetPasteImageId(use_dark)
                                     : GetUploadImageId(use_dark);

  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id);
}

SkColor DeepScanningDialogViews::GetSideImageLogoColor() const {
  const views::Widget* widget = GetWidget();
  DCHECK(widget);
  switch (dialog_status_) {
    case DeepScanningDialogStatus::PENDING:
      // Match the spinner in the pending state.
      return widget->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_ThrobberSpinningColor);
    case DeepScanningDialogStatus::SUCCESS:
    case DeepScanningDialogStatus::FAILURE:
    case DeepScanningDialogStatus::WARNING:
      // In a result state the background will have the result's color, so the
      // logo should have the same color as the background.
      return GetBackgroundColor(widget);
  }
}

// static
void DeepScanningDialogViews::SetMinimumPendingDialogTimeForTesting(
    base::TimeDelta delta) {
  minimum_pending_dialog_time_ = delta;
}

// static
void DeepScanningDialogViews::SetSuccessDialogTimeoutForTesting(
    base::TimeDelta delta) {
  success_dialog_timeout_ = delta;
}

// static
void DeepScanningDialogViews::SetObserverForTesting(TestObserver* observer) {
  observer_for_testing = observer;
}

views::ImageView* DeepScanningDialogViews::GetTopImageForTesting() const {
  return image_;
}

views::Throbber* DeepScanningDialogViews::GetSideIconSpinnerForTesting() const {
  return side_icon_spinner_;
}

views::Label* DeepScanningDialogViews::GetMessageForTesting() const {
  return message_;
}

}  // namespace safe_browsing
