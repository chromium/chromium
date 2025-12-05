// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_feature_tour_dialog_view.h"

#include "ash/quick_insert/resources/grit/quick_insert_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
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

std::u16string GetBodyText(
    QuickInsertFeatureTourDialogView::EditorStatus editor_status) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (editor_status) {
    case QuickInsertFeatureTourDialogView::EditorStatus::kEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITH_EDITOR_BODY_TEXT);
    case QuickInsertFeatureTourDialogView::EditorStatus::kNotEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITHOUT_EDITOR_BODY_TEXT);
  }
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

ui::ImageModel GetIllustration() {
  return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
      IDR_QUICK_INSERT_FEATURE_TOUR_ILLUSTRATION);
}

}  // namespace

QuickInsertFeatureTourDialogView::QuickInsertFeatureTourDialogView(
    EditorStatus editor_status,
    base::OnceClosure learn_more_callback,
    base::OnceClosure completion_callback) {
  views::Builder<QuickInsertFeatureTourDialogView>(this)
      .SetBorder(views::CreatePaddedBorder(
          std::make_unique<views::HighlightBorder>(
              kFeatureTourDialogCornerRadius,
              views::HighlightBorder::Type::kHighlightBorderOnShadow),
          kFeatureTourDialogBorderInsets))
      .SetTitleText(GetHeadingText(editor_status))
      .SetDescription(GetBodyText(editor_status))
      .SetAcceptButtonText(
          l10n_util::GetStringUTF16(IDS_PICKER_FEATURE_TOUR_START_BUTTON_LABEL))
      .SetAcceptCallback(std::move(completion_callback))
      .SetCancelButtonText(l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_LEARN_MORE_BUTTON_LABEL))
      .SetCancelCallback(std::move(learn_more_callback))
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
      .SetModalType(ui::mojom::ModalType::kSystem)
      .BuildChildren();

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

const views::Button*
QuickInsertFeatureTourDialogView::learn_more_button_for_testing() const {
  return GetCancelButtonForTesting();  // IN-TEST
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
