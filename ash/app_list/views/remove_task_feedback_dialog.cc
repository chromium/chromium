// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_task_feedback_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDialogWidth = 360;
constexpr gfx::Insets kDialogContentInsets = gfx::Insets::VH(20, 24);
constexpr float kDialogRoundedCornerRadius = 16.0f;
constexpr int kDialogShadowElevation = 3;
constexpr int kMarginBetweenTitleAndBody = 8;
constexpr int kMarginBetweenBodyAndOptions = 8;
constexpr int kMarginBetweenOptionsAndButtons = 20;
constexpr int kMarginBetweenBorderAndCheckboxes = 20;
constexpr int kMarginBetweenButtons = 8;

}  // namespace

RemoveTaskFeedbackDialog::RemoveTaskFeedbackDialog(
    ConfirmDialogCallback confirm_callback,
    ContinueTaskView::TaskResultType task_type)
    : confirm_callback_(std::move(confirm_callback)), task_type_(task_type) {
  views::View* button_row;
  views::Builder<RemoveTaskFeedbackDialog>(this)
      .SetModalType(ui::MODAL_TYPE_WINDOW)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kDialogContentInsets))
      .SetPaintToLayer()
      .AddChildren(
          views::Builder<views::Label>()
              .CopyAddressTo(&title_)
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_TITLE))
              .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
              .SetTextStyle(ash::STYLE_EMPHASIZED)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetAutoColorReadabilityEnabled(false)
              .SetPaintToLayer(),
          views::Builder<views::Label>()
              .CopyAddressTo(&feedback_text_)
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_FEEDBACK_TEXT))
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(kMarginBetweenTitleAndBody, 0,
                                             kMarginBetweenBodyAndOptions, 0))
              .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetAutoColorReadabilityEnabled(false)
              .SetPaintToLayer(),
          views::Builder<views::View>()
              .SetLayoutManager(std::make_unique<views::BoxLayout>(
                  views::BoxLayout::Orientation::kVertical))
              .AddChildren(
                  views::Builder<views::RadioButton>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_ANY_SUGGESTIONS_OPTION))
                      .CopyAddressTo(&all_suggestions_option_),
                  views::Builder<views::RadioButton>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_THIS_SUGGESTION_OPTION))
                      .CopyAddressTo(&single_suggestion_option_),
                  views::Builder<views::View>()
                      .CopyAddressTo(&secondary_options_panel_)
                      .SetLayoutManager(std::make_unique<views::BoxLayout>(
                          views::BoxLayout::Orientation::kVertical))
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(kMarginBetweenBodyAndOptions,
                                            kMarginBetweenBorderAndCheckboxes,
                                            kMarginBetweenOptionsAndButtons, 0))
                      .SetVisible(false)
                      .AddChildren(
                          views::Builder<views::Checkbox>()
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_FILE_DONE_OPTION))
                              .CopyAddressTo(&done_using_option_),
                          views::Builder<views::Checkbox>()
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_DO_NOT_SHOW_FILE_OPTION))
                              .CopyAddressTo(&not_show_option_))),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
              .SetBetweenChildSpacing(kMarginBetweenButtons)
              .CopyAddressTo(&button_row))
      .BuildChildren();

  layer()->SetFillsBoundsOpaquely(false);
  title_->layer()->SetFillsBoundsOpaquely(false);
  feedback_text_->layer()->SetFillsBoundsOpaquely(false);

  cancel_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
      views::Button::PressedCallback(base::BindRepeating(
          &RemoveTaskFeedbackDialog::Cancel, base::Unretained(this))),
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_CANCEL_BUTTON_LABEL),
      PillButton::Type::kIconless, nullptr));
  remove_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
      views::Button::PressedCallback(base::BindRepeating(
          &RemoveTaskFeedbackDialog::Remove, base::Unretained(this))),
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_CONTINUE_SECTION_REMOVE_DIALOG_REMOVE_BUTTON_LABEL),
      PillButton::Type::kIconlessProminent, nullptr));

  single_suggestion_option_subscription_ =
      single_suggestion_option_->AddCheckedChangedCallback(base::BindRepeating(
          &RemoveTaskFeedbackDialog::ToggleSecondaryOptionsPanel,
          base::Unretained(this)));

  view_shadow_ = std::make_unique<ViewShadow>(this, kDialogShadowElevation);
  view_shadow_->SetRoundedCornerRadius(kDialogRoundedCornerRadius);

  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
}

RemoveTaskFeedbackDialog::~RemoveTaskFeedbackDialog() = default;

gfx::Size RemoveTaskFeedbackDialog::CalculatePreferredSize() const {
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}

void RemoveTaskFeedbackDialog::OnThemeChanged() {
  views::WidgetDelegateView::OnThemeChanged();

  title_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  feedback_text_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  auto set_checkbox_text_color = [](views::Checkbox* view) {
    view->SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  };
  set_checkbox_text_color(all_suggestions_option_);
  set_checkbox_text_color(single_suggestion_option_);
  set_checkbox_text_color(done_using_option_);
  set_checkbox_text_color(not_show_option_);

  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80),
      kDialogRoundedCornerRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kDialogRoundedCornerRadius,
      views::HighlightBorder::Type::kHighlightBorder1,
      /*use_light_colors=*/false));
}

void RemoveTaskFeedbackDialog::Remove() {
  const bool has_feedback = all_suggestions_option_->GetChecked() ||
                            single_suggestion_option_->GetChecked();

  if (has_feedback)
    LogMetricsOnFeedbackSubmitted();

  if (confirm_callback_)
    std::move(confirm_callback_).Run(has_feedback);

  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

void RemoveTaskFeedbackDialog::Cancel() {
  confirm_callback_.Reset();
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
}

void RemoveTaskFeedbackDialog::ToggleSecondaryOptionsPanel() {
  secondary_options_panel_->SetVisible(single_suggestion_option_->GetChecked());
  PreferredSizeChanged();
}

RemoveTaskFeedbackDialog::FeedbackBuckets
RemoveTaskFeedbackDialog::GetFeedbackBucketValue() {
  DCHECK_NE(task_type_, ContinueTaskView::TaskResultType::kUnknown);

  const bool is_local_file =
      task_type_ == ContinueTaskView::TaskResultType::kLocalFile;

  if (all_suggestions_option_->GetChecked()) {
    return is_local_file ? FeedbackBuckets::kLocalFileDontWantAny
                         : FeedbackBuckets::kDriveFileDontWantAny;
  }

  if (single_suggestion_option_->GetChecked()) {
    const bool done_using_checked = done_using_option_->GetChecked();
    const bool not_show_checked = not_show_option_->GetChecked();
    if (done_using_checked && not_show_checked) {
      return is_local_file ? FeedbackBuckets::kLocalFileDontNeedDontSee
                           : FeedbackBuckets::kDriveFileDontNeedDontSee;
    }

    if (done_using_checked) {
      return is_local_file ? FeedbackBuckets::kLocalFileDontNeed
                           : FeedbackBuckets::kDriveFileDontNeed;
    }

    if (not_show_checked) {
      return is_local_file ? FeedbackBuckets::kLocalFileDontSee
                           : FeedbackBuckets::kDriveFileDontSee;
    }

    return is_local_file ? FeedbackBuckets::kLocalFileDontWantThis
                         : FeedbackBuckets::kDriveFileDontWantThis;
  }
  NOTREACHED();
  return FeedbackBuckets::kInvalidFeedback;
}

void RemoveTaskFeedbackDialog::LogMetricsOnFeedbackSubmitted() {
  base::UmaHistogramEnumeration(
      "Apps.AppList.Search.ContinueResultRemovalReason",
      GetFeedbackBucketValue(), FeedbackBuckets::kMaxValue);
}

BEGIN_METADATA(RemoveTaskFeedbackDialog, views::WidgetDelegateView)
END_METADATA

}  // namespace ash
