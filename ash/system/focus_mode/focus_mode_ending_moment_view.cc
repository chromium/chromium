// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_ending_moment_view.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr auto kTextContainerSize = gfx::Size(225, 72);
constexpr int kSpaceBetweenText = 4;
constexpr int kSpaceBetweenButtons = 8;
// The maximum width for the title is 202px, which is based on the width for the
// party-popper is 19px and the width for the space separator between the emoji
// and the title is 4px.
constexpr int kTitleMaximumWidth = 202;

std::unique_ptr<views::Label> CreateTextLabel(
    gfx::HorizontalAlignment alignment,
    TypographyToken token,
    ui::ColorId color_id,
    bool allow_multiline,
    const std::u16string& text) {
  auto label = std::make_unique<views::Label>();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(alignment);
  TypographyProvider::Get()->StyleLabel(token, *label);
  label->SetEnabledColorId(color_id);
  label->SetText(text);
  label->SetMultiLine(allow_multiline);
  label->SetMaxLines(allow_multiline ? 2 : 1);
  return label;
}

}  // namespace

FocusModeEndingMomentView::FocusModeEndingMomentView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // The main layout will be horizontal with the text container on the left,
  // and the button container on the right.
  SetOrientation(views::LayoutOrientation::kHorizontal);

  // Add a vertical container on the left for the text.
  auto* text_container = AddChildView(std::make_unique<views::BoxLayoutView>());
  text_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  text_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  text_container->SetBetweenChildSpacing(kSpaceBetweenText);
  text_container->SetPreferredSize(kTextContainerSize);
  text_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width =*/false));

  // `title_and_emoji_box` contains a congratulatory text in `title_label` and
  // an party-popper emoji.
  auto* title_and_emoji_box =
      text_container->AddChildView(std::make_unique<views::BoxLayoutView>());
  title_and_emoji_box->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  title_and_emoji_box->SetMainAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  title_and_emoji_box->SetBetweenChildSpacing(kSpaceBetweenText);

  auto* title_label = title_and_emoji_box->AddChildView(
      CreateTextLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosHeadline1,
                      cros_tokens::kCrosSysOnSurface, /*allow_multiline=*/false,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE)));
  title_label->SetMaximumWidthSingleLine(kTitleMaximumWidth);

  title_and_emoji_box->AddChildView(CreateTextLabel(
      gfx::ALIGN_LEFT, TypographyToken::kCrosHeadline1,
      cros_tokens::kCrosSysOnSurface, /*allow_multiline=*/false, u"🎉"));

  text_container->AddChildView(
      CreateTextLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosAnnotation1,
                      cros_tokens::kCrosSysOnSurface, /*allow_multiline=*/true,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_BODY)));

  // Add a top level spacer in first layout manager, between the text container
  // and button container.
  auto* spacer_view = AddChildView(std::make_unique<views::View>());
  spacer_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Add the vertical box layout for the button container that holds the "Done"
  // and "+10 min" buttons.
  auto* button_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  button_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  button_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  button_container->SetBetweenChildSpacing(kSpaceBetweenButtons);

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  button_container->SetLayoutManagerUseConstrainedSpace(false);

  auto* focus_mode_controller = FocusModeController::Get();
  button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&FocusModeController::ResetFocusSession,
                          base::Unretained(focus_mode_controller)),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_DONE_BUTTON),
      PillButton::Type::kPrimaryWithoutIcon, /*icon=*/nullptr));

  extend_session_duration_button_ =
      button_container->AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&FocusModeController::ExtendSessionDuration,
                              base::Unretained(focus_mode_controller)),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_FOCUS_MODE_EXTEND_TEN_MINUTES_BUTTON_LABEL),
          PillButton::Type::kSecondaryWithoutIcon,
          /*icon=*/nullptr));
  extend_session_duration_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_INCREASE_TEN_MINUTES_BUTTON_ACCESSIBLE_NAME));
}

void FocusModeEndingMomentView::SetExtendButtonEnabled(bool enabled) {
  extend_session_duration_button_->SetEnabled(enabled);
}

BEGIN_METADATA(FocusModeEndingMomentView)
END_METADATA

}  // namespace ash
