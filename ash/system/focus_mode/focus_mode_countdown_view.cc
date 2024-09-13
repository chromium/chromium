// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_countdown_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr int kCountdownViewHeight = 62;
constexpr int kSpaceBetweenButtons = 8;
constexpr int kBarWidth = 225;
constexpr int kBarHeight = 8;
constexpr int kAboveBarSpace = 8;
constexpr int kAboveBarSpaceInBubble = 12;
constexpr int kBelowBarSpace = 8;

std::unique_ptr<views::Label> CreateTimerLabel(
    gfx::HorizontalAlignment alignment,
    TypographyToken token,
    ui::ColorId color_id) {
  auto label = std::make_unique<views::Label>();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(alignment);
  TypographyProvider::Get()->StyleLabel(token, *label);
  label->SetEnabledColorId(color_id);
  return label;
}

std::unique_ptr<views::View> CreateSpacerView() {
  auto spacer_view = std::make_unique<views::View>();
  spacer_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  return spacer_view;
}

}  // namespace

FocusModeCountdownView::FocusModeCountdownView(bool include_end_button)
    : include_end_button_(include_end_button) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // The main layout will be horizontal with the timer container on the left,
  // and the button container on the right.
  SetOrientation(views::LayoutOrientation::kHorizontal);

  // Add a vertical container on the left for the countdown timer, the progress
  // bar, and the bar label container.
  auto* timer_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  timer_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  timer_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  timer_container->SetPreferredSize(gfx::Size(kBarWidth, kCountdownViewHeight));
  timer_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width =*/false));

  time_remaining_label_ = timer_container->AddChildView(
      CreateTimerLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosDisplay6Regular,
                       cros_tokens::kCrosSysOnSurface));

  // TODO(b/286931547): Timer Progress Bar.
  progress_bar_ =
      timer_container->AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_->SetPreferredHeight(kBarHeight);
  progress_bar_->SetPreferredCornerRadii(gfx::RoundedCornersF(kBarHeight / 2));
  progress_bar_->SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase);
  progress_bar_->SetForegroundColorId(cros_tokens::kCrosSysPrimary);
  progress_bar_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      include_end_button_ ? kAboveBarSpaceInBubble : kAboveBarSpace, 0,
      kBelowBarSpace, 0)));

  // Add a horizontal container to hold the two bar label timers, and the spacer
  // view used to space them out.
  auto* bar_label_container =
      timer_container->AddChildView(std::make_unique<views::FlexLayoutView>());
  bar_label_container->SetOrientation(views::LayoutOrientation::kHorizontal);

  time_elapsed_label_ = bar_label_container->AddChildView(
      CreateTimerLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosLabel1,
                       cros_tokens::kCrosSysSecondary));

  bar_label_container->AddChildView(CreateSpacerView());

  time_total_label_ = bar_label_container->AddChildView(
      CreateTimerLabel(gfx::ALIGN_RIGHT, TypographyToken::kCrosLabel2,
                       cros_tokens::kCrosSysSecondary));

  // Add a top level spacer in first layout manager, between the timer container
  // and button container.
  AddChildView(CreateSpacerView());

  // Add the vertical box layout for the button container that holds the "End"
  // and "+10 min" buttons.
  auto* button_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  button_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  button_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  button_container->SetBetweenChildSpacing(kSpaceBetweenButtons);

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  button_container->SetLayoutManagerUseConstrainedSpace(false);

  FocusModeController* focus_mode_controller = FocusModeController::Get();
  if (include_end_button_) {
    end_button_ = button_container->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(
            &FocusModeController::ToggleFocusMode,
            base::Unretained(focus_mode_controller),
            focus_mode_histogram_names::ToggleSource::kContextualPanel),
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON_LABEL),
        PillButton::Type::kPrimaryWithoutIcon, /*icon=*/nullptr));
    end_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON_ACCESSIBLE_NAME));
  }

  extend_session_duration_button_ =
      button_container->AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&FocusModeController::ExtendSessionDuration,
                              base::Unretained(focus_mode_controller)),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_FOCUS_MODE_EXTEND_TEN_MINUTES_BUTTON_LABEL),
          include_end_button_ ? PillButton::Type::kSecondaryWithoutIcon
                              : PillButton::Type::kSecondaryLargeWithoutIcon,
          /*icon=*/nullptr));
  extend_session_duration_button_->SetUseLabelAsDefaultTooltip(false);
  extend_session_duration_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_INCREASE_TEN_MINUTES_BUTTON_ACCESSIBLE_NAME));
  views::InkDrop::Get(extend_session_duration_button_)
      ->SetMode(views::InkDropHost::InkDropMode::OFF);
}

void FocusModeCountdownView::UpdateUI(
    const FocusModeSession::Snapshot& session_snapshot) {
  CHECK_EQ(session_snapshot.state, FocusModeSession::State::kOn);

  time_remaining_label_->SetText(focus_mode_util::GetDurationString(
      session_snapshot.remaining_time, /*digital_format=*/true));
  time_total_label_->SetText(focus_mode_util::GetDurationString(
      session_snapshot.session_duration, /*digital_format=*/true));
  time_elapsed_label_->SetText(focus_mode_util::GetDurationString(
      session_snapshot.time_elapsed, /*digital_format=*/true));
  progress_bar_->SetValue(session_snapshot.progress);

  const bool session_extendable =
      FocusModeController::CanExtendSessionDuration(session_snapshot);
  // Clear the focus if we are disabling the extend button and it has focus.
  if (extend_session_duration_button_->HasFocus() && !session_extendable) {
    // Release focus so that disabling `extend_session_duration_button_` below
    // does not shift focus into the next available view automatically.
    auto* focus_manager = GetFocusManager();
    focus_manager->ClearFocus();
    focus_manager->SetStoredFocusView(nullptr);
  }

  extend_session_duration_button_->SetEnabled(session_extendable);
}

BEGIN_METADATA(FocusModeCountdownView)
END_METADATA

}  // namespace ash
