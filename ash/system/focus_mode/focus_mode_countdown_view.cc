// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_countdown_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr int kCountdownViewHeight = 74;
constexpr int kButtonWidth = 79;
constexpr int kSpaceBetweenButtons = 10;
constexpr int kBarWidth = 200;
constexpr int kBarHeight = 8;
constexpr int kAboveBarSpace = 14;
constexpr int kBelowBarSpace = 8;

// The gap between the progress bar and the buttons.
constexpr int kBarGapHorizontal = 50;

std::unique_ptr<views::Label> CreateTimerLabel(
    gfx::HorizontalAlignment alignment,
    TypographyToken token) {
  auto label = std::make_unique<views::Label>();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(alignment);
  TypographyProvider::Get()->StyleLabel(token, *label);
  return label;
}

}  // namespace

FocusModeCountdownView::FocusModeCountdownView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // The main layout will be horizontal with the timer container on the left,
  // and the button container on the right.
  SetOrientation(views::LayoutOrientation::kHorizontal);

  // Add a vertical container on the left for the countdown timer, the progress
  // bar, and the bar label container.
  auto* timer_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  timer_container->SetOrientation(views::LayoutOrientation::kVertical);
  timer_container->SetPreferredSize(gfx::Size(kBarWidth, kCountdownViewHeight));

  time_remaining_label_ = timer_container->AddChildView(
      CreateTimerLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosDisplay6Regular));

  // TODO(b/286931547): Timer Progress Bar
  progress_bar_ =
      timer_container->AddChildView(std::make_unique<views::ProgressBar>(
          /*preferred_height=*/kBarHeight, /*allow_round_corner*/ true));
  progress_bar_->SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase);
  progress_bar_->SetForegroundColorId(cros_tokens::kCrosSysPrimary);
  progress_bar_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kAboveBarSpace, 0, kBelowBarSpace, 0)));

  // Add a horizontal container to hold the two bar label timers, and the spacer
  // view used to space them out.
  auto* bar_label_container =
      timer_container->AddChildView(std::make_unique<views::FlexLayoutView>());
  bar_label_container->SetOrientation(views::LayoutOrientation::kHorizontal);

  time_elapsed_label_ = bar_label_container->AddChildView(
      CreateTimerLabel(gfx::ALIGN_LEFT, TypographyToken::kCrosLabel1));

  auto* bar_label_spacer_view =
      bar_label_container->AddChildView(std::make_unique<views::View>());
  bar_label_spacer_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  time_total_label_ = bar_label_container->AddChildView(
      CreateTimerLabel(gfx::ALIGN_RIGHT, TypographyToken::kCrosLabel2));

  // Add the vertical box layout for the button container that holds the "End"
  // and "+10 min" buttons.
  auto* button_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  button_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  button_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  button_container->SetBetweenChildSpacing(kSpaceBetweenButtons);
  button_container->SetMinimumCrossAxisSize(kButtonWidth);
  button_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, kBarGapHorizontal, 0, 0)));

  FocusModeController* focus_mode_controller = FocusModeController::Get();
  button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&FocusModeController::ToggleFocusMode,
                          base::Unretained(focus_mode_controller)),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON),
      PillButton::Type::kPrimaryWithoutIcon, /*icon=*/nullptr));

  button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&FocusModeController::ExtendActiveSessionDuration,
                          base::Unretained(focus_mode_controller)),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_EXTEND_TEN_MINUTES_BUTTON_LABEL),
      PillButton::Type::kSecondaryWithoutIcon,
      /*icon=*/nullptr));

  focus_mode_controller->AddObserver(this);

  // Set the label texts.
  UpdateUI();
}

FocusModeCountdownView::~FocusModeCountdownView() {
  FocusModeController::Get()->RemoveObserver(this);
}

void FocusModeCountdownView::OnTimerTick() {
  UpdateUI();
}

void FocusModeCountdownView::UpdateUI() {
  auto* controller = FocusModeController::Get();
  CHECK(controller->in_focus_session());

  const base::TimeDelta time_remaining =
      controller->end_time() - base::Time::Now();
  time_remaining_label_->SetText(focus_mode_util::GetDurationString(
      time_remaining, focus_mode_util::TimeFormatType::kFull));

  const base::TimeDelta session_duration = controller->session_duration();
  time_total_label_->SetText(focus_mode_util::GetDurationString(
      session_duration, focus_mode_util::TimeFormatType::kDigital));

  const base::TimeDelta time_elapsed = session_duration - time_remaining;
  time_elapsed_label_->SetText(focus_mode_util::GetDurationString(
      time_elapsed, focus_mode_util::TimeFormatType::kDigital));

  progress_bar_->SetValue(time_elapsed / session_duration);
}

}  // namespace ash
