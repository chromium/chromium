// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_ending_moment_view.h"

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
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr auto kTextContainerSize = gfx::Size(225, 72);
constexpr int kSpaceBetweenText = 4;
constexpr int kSpaceBetweenButtons = 8;

std::unique_ptr<views::Label> CreateTextLabel(
    gfx::HorizontalAlignment alignment,
    TypographyToken token,
    ui::ColorId color_id,
    bool allow_multiline,
    int message_id) {
  auto label = std::make_unique<views::Label>();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(alignment);
  TypographyProvider::Get()->StyleLabel(token, *label);
  label->SetEnabledColorId(color_id);
  label->SetText(l10n_util::GetStringUTF16(message_id));
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
      views::BoxLayout::MainAxisAlignment::kCenter);
  text_container->SetBetweenChildSpacing(kSpaceBetweenText);
  text_container->SetPreferredSize(kTextContainerSize);
  text_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width =*/false));

  auto* focus_mode_controller = FocusModeController::Get();
  text_container->AddChildView(
      CreateTextLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosHeadline1,
                      cros_tokens::kCrosSysOnSurface, /*allow_multiline=*/false,
                      IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE));
  text_container->AddChildView(CreateTextLabel(
      gfx::ALIGN_LEFT, TypographyToken::kCrosAnnotation1,
      cros_tokens::kCrosSysOnSurface, /*allow_multiline=*/true,
      focus_mode_controller->selected_task_title().empty()
          ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_BODY
          : IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_BODY_WITH_TASK));

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

  button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&FocusModeController::ResetFocusSession,
                          base::Unretained(focus_mode_controller)),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_DONE_BUTTON),
      PillButton::Type::kPrimaryWithoutIcon, /*icon=*/nullptr));

  extend_session_duration_button_ =
      button_container->AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&FocusModeController::ExtendExpiredSession,
                              base::Unretained(focus_mode_controller)),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_FOCUS_MODE_EXTEND_TEN_MINUTES_BUTTON_LABEL),
          PillButton::Type::kSecondaryWithoutIcon,
          /*icon=*/nullptr));
}

void FocusModeEndingMomentView::SetExtendButtonEnabled(bool enabled) {
  extend_session_duration_button_->SetEnabled(enabled);
}

BEGIN_METADATA(FocusModeEndingMomentView)
END_METADATA

}  // namespace ash
