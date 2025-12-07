// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_feature_tour_dialog_view.h"

#include "ash/quick_insert/resources/grit/quick_insert_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
#include "build/branding_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr auto kFeatureTourDialogBorderInsets =
    gfx::Insets::TLBR(0, 32, 28, 32);

constexpr int kFeatureTourDialogCornerRadius = 20;
constexpr auto kFeatureTourDialogIllustrationCornerRadii =
    gfx::RoundedCornersF(/*upper_left=*/kFeatureTourDialogCornerRadius,
                         /*upper_right=*/kFeatureTourDialogCornerRadius,
                         /*lower_right=*/0,
                         /*lower_left=*/0);

constexpr auto kCloseButtonInsets = gfx::Insets::TLBR(8, 0, 0, 8);

views::Builder<views::StyledLabel> GetTextBodyBuilder() {
  // Copied from
  // https://crsrc.org/c/ash/capture_mode/disclaimer_view.cc;l=95;drc=f4a1156e602a7d751e24f70e30c90695098aaea9
  return views::Builder<views::StyledLabel>()
      .SetDefaultTextStyle(views::style::TextStyle::STYLE_BODY_2)
      .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

views::StyledLabel::RangeStyleInfo GetLinkTextStyle(
    base::RepeatingClosure callback) {
  // Copied from
  // https://crsrc.org/c/ash/capture_mode/disclaimer_view.cc;l=131;drc=f4a1156e602a7d751e24f70e30c90695098aaea9
  auto info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(std::move(callback));
  info.text_style = views::style::STYLE_LINK_2;
  return info;
}

std::u16string GetHeadingText(
    QuickInsertFeatureTourDialogView::EditorStatus editor_status) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (editor_status) {
    case QuickInsertFeatureTourDialogView::EditorStatus::kEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITH_EDITOR_HEADING_TEXT);
    case QuickInsertFeatureTourDialogView::EditorStatus::kNotEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITHOUT_EDITOR_HEADING_TEXT);
  }
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

int GetBodyTextMessageId(
    QuickInsertFeatureTourDialogView::EditorStatus editor_status) {
  switch (editor_status) {
    case QuickInsertFeatureTourDialogView::EditorStatus::kEligible:
      return IDS_QUICK_INSERT_FEATURE_TOUR_WITH_EDITOR_BODY_TEXT;
    case QuickInsertFeatureTourDialogView::EditorStatus::kNotEligible:
      return IDS_QUICK_INSERT_FEATURE_TOUR_WITHOUT_EDITOR_BODY_TEXT;
  }
}

views::Builder<views::StyledLabel> CreateMiddleContentView(
    QuickInsertFeatureTourDialogView::EditorStatus editor_status,
    base::RepeatingClosure press_link_callback) {
  std::vector<size_t> offsets;
  const std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_PICKER_FEATURE_TOUR_LEARN_MORE_BUTTON_LABEL);
  const std::u16string text = l10n_util::GetStringFUTF16(
      GetBodyTextMessageId(editor_status), {link_text}, &offsets);

  return GetTextBodyBuilder()
      .SetText(text)
      // TODO: https://crbug.com/465882905 - Temporary workaround to
      // remove margins automatically created by SystemDialogDelegateView
      // Remove this once QuickInsertFeatureTourDialogView is no longer a
      // SystemDialogDelegateView.
      .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(1, 0, 0, 0))
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle(std::move(press_link_callback)));
}

ui::ImageModel GetIllustration() {
  return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
      IDR_QUICK_INSERT_FEATURE_TOUR_ILLUSTRATION);
}

}  // namespace

QuickInsertFeatureTourDialogView::QuickInsertFeatureTourDialogView(
    EditorStatus editor_status,
    base::RepeatingClosure learn_more_callback,
    base::OnceClosure completion_callback) {
  views::Builder<QuickInsertFeatureTourDialogView>(this)
      .SetBorder(views::CreatePaddedBorder(
          std::make_unique<views::HighlightBorder>(
              kFeatureTourDialogCornerRadius,
              views::HighlightBorder::Type::kHighlightBorderOnShadow),
          kFeatureTourDialogBorderInsets))
      .SetTitleText(GetHeadingText(editor_status))
      .SetMiddleContentView(
          CreateMiddleContentView(
              editor_status, base::BindRepeating(
                                 &QuickInsertFeatureTourDialogView::CloseWidget,
                                 base::Unretained(this))
                                 .Then(std::move(learn_more_callback)))
              .CopyAddressTo(&body_text_for_testing_))
      .SetMiddleContentAlignment(views::LayoutAlignment::kStretch)
      .SetAcceptButtonText(
          l10n_util::GetStringUTF16(IDS_PICKER_FEATURE_TOUR_START_BUTTON_LABEL))
      .SetAcceptCallback(std::move(completion_callback))
      .SetTopContentView(
          views::Builder<views::View>()
              .SetUseDefaultFillLayout(true)
              .AddChildren(
                  views::Builder<views::ImageView>()
                      .SetBackground(views::CreateRoundedRectBackground(
                          cros_tokens::kCrosSysIlloColor12,
                          kFeatureTourDialogIllustrationCornerRadii))
                      .SetImage(GetIllustration()),
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetMainAxisAlignment(
                          views::BoxLayout::MainAxisAlignment::kEnd)
                      .SetCrossAxisAlignment(
                          views::BoxLayout::CrossAxisAlignment::kStart)
                      .SetInsideBorderInsets(kCloseButtonInsets)
                      .AddChild(
                          views::Builder<views::Button>(
                              std::make_unique<CloseButton>(
                                  base::BindOnce(
                                      &QuickInsertFeatureTourDialogView::
                                          CloseWidget,
                                      weak_ptr_factory_.GetWeakPtr()),
                                  CloseButton::Type::kLargeFloating))
                              .CopyAddressTo(&close_button_for_testing_))))
      .BuildChildren();
  SetCancelButtonVisible(false);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

QuickInsertFeatureTourDialogView::~QuickInsertFeatureTourDialogView() = default;

bool QuickInsertFeatureTourDialogView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  if (views::Widget* widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
    // Don't propagate.
    return true;
  }
  return false;
}

const views::Link*
QuickInsertFeatureTourDialogView::learn_more_link_for_testing() const {
  return body_text_for_testing_->GetFirstLinkForTesting();  // IN-TEST
}

const views::Button*
QuickInsertFeatureTourDialogView::complete_button_for_testing() const {
  return GetAcceptButtonForTesting();  // IN-TEST
}

const views::Button*
QuickInsertFeatureTourDialogView::close_button_for_testing() const {
  return close_button_for_testing_;  // IN-TEST
}

void QuickInsertFeatureTourDialogView::CloseWidget() {
  if (views::Widget* widget = GetWidget()) {
    widget->Close();
  }
}

BEGIN_METADATA(QuickInsertFeatureTourDialogView)
END_METADATA

}  // namespace ash
