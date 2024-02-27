// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_caps_nudge_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr ui::ColorId kBackgroundColor = cros_tokens::kCrosSysSystemOnBase;
constexpr int kBorderRadius = 16;
constexpr int kPadding = 8;
constexpr int kLeftIconSize = 48;
constexpr int kRightPadding = 16;
constexpr int kKeyIconSize = 14;
constexpr int kPadingAroundSmallIcon = 4;
constexpr int kPaddingBetweenItems = 8;

}  // namespace

PickerCapsNudgeView::PickerCapsNudgeView(
    views::Button::PressedCallback hide_callback) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // LHS - Random placeholder icon.
  const ui::ImageModel icon = ui::ImageModel::FromVectorIcon(
      vector_icons::kForwardArrowIcon, ui::kColorAvatarIconIncognito,
      kLeftIconSize);
  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(icon)
                   .SetBorder(views::CreateEmptyBorder(
                       gfx::Insets::TLBR(0, 0, 0, kPaddingBetweenItems)))
                   .Build());

  // Central nudge text.
  // TODO(b/323413906): translate all strings below.
  auto* nudge_text_container =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kVertical)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                       .Build());

  // Allow the nudge text to grow to ensure good padding between text and the
  // buttons.
  nudge_text_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  nudge_text_container->AddChildView(ash::bubble_utils::CreateLabel(
      TypographyToken::kCrosButton2, u"Looking for Caps Lock",
      cros_tokens::kCrosSysOnSurface));

  auto* secondary_text = nudge_text_container->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCollapseMargins(true)
          .SetIgnoreDefaultMainAxisMargins(true)
          .Build());
  secondary_text->SetDefault(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kPadingAroundSmallIcon, 0, kPadingAroundSmallIcon));
  secondary_text->AddChildView(ash::bubble_utils::CreateLabel(
      TypographyToken::kCrosAnnotation1, u"Hold the",
      cros_tokens::kCrosSysSecondary));

  const ui::ImageModel key_icon = ui::ImageModel::FromVectorIcon(
      vector_icons::kForwardArrowIcon, cros_tokens::kCrosSysSecondary,
      kKeyIconSize);
  secondary_text->AddChildView(
      views::Builder<views::ImageView>().SetImage(key_icon).Build());
  secondary_text->AddChildView(
      ash::bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation1, u"key",
                                     cros_tokens::kCrosSysSecondary));

  // RHS - OK pill button.
  ok_button_ = AddChildView(
      views::Builder<ash::PillButton>()
          .SetText(u"OK")
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets::TLBR(0, kPaddingBetweenItems, 0, 0)))
          .SetPillButtonType(ash::PillButton::Type::kDefaultElevatedWithoutIcon)
          .SetCallback(std::move(hide_callback))
          .Build());

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kPadding, kPadding, kPadding, kRightPadding)));
  SetBackground(views::CreateThemedRoundedRectBackground(kBackgroundColor,
                                                         kBorderRadius));
  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(kPaddingBetweenItems, kPaddingBetweenItems, 0,
                                kPaddingBetweenItems));
}

PickerCapsNudgeView::~PickerCapsNudgeView() = default;

BEGIN_METADATA(PickerCapsNudgeView)
END_METADATA
}  // namespace ash
