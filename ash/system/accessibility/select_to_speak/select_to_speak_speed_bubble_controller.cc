// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_bubble_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_utils.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_constants.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr int kPreferredWidth = 150;
constexpr int kBubbleViewMargin = 2;

}  // namespace

SelectToSpeakSpeedBubbleController::SelectToSpeakSpeedBubbleController(
    SelectToSpeakSpeedView::Delegate* delegate)
    : delegate_(delegate) {
  Shell::Get()->activation_client()->AddObserver(this);
}

SelectToSpeakSpeedBubbleController::~SelectToSpeakSpeedBubbleController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  MaybeRecordDurationHistogram();
}

void SelectToSpeakSpeedBubbleController::Show(views::View* anchor_view,
                                              double speech_rate) {
  DCHECK(anchor_view);
  if (!bubble_widget_) {
    TrayBubbleView::InitParams init_params;
    init_params.delegate = GetWeakPtr();
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kView;
    init_params.anchor_view = anchor_view;
    init_params.is_anchored_to_status_area = false;
    init_params.margin = gfx::Insets::VH(kBubbleViewMargin, kBubbleViewMargin);
    init_params.translucent = true;
    init_params.close_on_deactivate = false;
    init_params.preferred_width = kPreferredWidth;
    init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;

    bubble_view_ = new TrayBubbleView(init_params);
    bubble_view_->SetArrow(views::BubbleBorder::BOTTOM_RIGHT);
    bubble_view_->SetCanActivate(true);
    bubble_view_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    speed_view_ = new SelectToSpeakSpeedView(this, speech_rate);
    bubble_view_->AddChildView(speed_view_.get());

    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    bubble_widget_->GetNativeView()->SetName(
        kSelectToSpeakSpeedBubbleWindowName);
    TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
    bubble_view_->InitializeAndShowBubble();
  } else {
    speed_view_->SetInitialSpeechRate(speech_rate);
  }

  bubble_view_->ChangeAnchorView(anchor_view);
  bubble_widget_->Show();
  if (last_show_time_ == base::Time()) {
    last_show_time_ = base::Time::Now();
  }
}

void SelectToSpeakSpeedBubbleController::Hide() {
  if (!bubble_widget_)
    return;
  bubble_widget_->Hide();
  MaybeRecordDurationHistogram();
}

void SelectToSpeakSpeedBubbleController::MaybeRecordDurationHistogram() {
  if (last_show_time_ == base::Time()) {
    return;
  }
  base::UmaHistogramTimes(
      "Accessibility.CrosSelectToSpeak.SpeedBubbleVisibleDuration",
      base::Time::Now() - last_show_time_);
  last_show_time_ = base::Time();
}

bool SelectToSpeakSpeedBubbleController::IsVisible() const {
  return bubble_widget_ && bubble_widget_->IsVisible();
}

std::u16string
SelectToSpeakSpeedBubbleController::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_SPEED_MENU);
}

void SelectToSpeakSpeedBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
}

void SelectToSpeakSpeedBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {}

void SelectToSpeakSpeedBubbleController::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active || !bubble_widget_ || !speed_view_)
    return;

  views::Widget* gained_widget =
      views::Widget::GetWidgetForNativeView(gained_active);
  if (gained_widget == bubble_widget_) {
    speed_view_->SetInitialFocus();
  }
}

void SelectToSpeakSpeedBubbleController::OnSpeechRateSelected(
    double speech_rate) {
  // Let parent handle this, so menu bubble controller can properly set speed
  // button state.
  delegate_->OnSpeechRateSelected(speech_rate);
}

}  // namespace ash
