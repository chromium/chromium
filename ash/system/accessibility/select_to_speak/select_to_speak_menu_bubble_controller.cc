// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_utils.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_constants.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_functions.h"
#include "select_to_speak_menu_bubble_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

const int kAnchorRectVerticalSpacing = 12;
const int kPreferredWidth = 368;

}  // namespace

SelectToSpeakMenuBubbleController::SelectToSpeakMenuBubbleController() {
  Shell::Get()->activation_client()->AddObserver(this);
}

SelectToSpeakMenuBubbleController::~SelectToSpeakMenuBubbleController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  MaybeRecordDurationHistogram();
}

void SelectToSpeakMenuBubbleController::Show(const gfx::Rect& anchor,
                                             bool is_paused,
                                             double initial_speech_rate) {
  initial_speech_rate_ = initial_speech_rate;
  if (!bubble_widget_) {
    TrayBubbleView::InitParams init_params;
    init_params.delegate = GetWeakPtr();
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.is_anchored_to_status_area = false;
    init_params.insets =
        gfx::Insets::VH(kBubbleMenuPadding, kBubbleMenuPadding);
    init_params.translucent = true;
    init_params.preferred_width = kPreferredWidth;
    init_params.close_on_deactivate = false;
    init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;

    bubble_view_ = new TrayBubbleView(init_params);
    bubble_view_->SetArrow(views::BubbleBorder::TOP_LEFT);
    bubble_view_->SetCanActivate(true);

    menu_view_ = new SelectToSpeakMenuView(this);
    menu_view_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kUnifiedTopShortcutSpacing, 0, 0, 0)));
    bubble_view_->AddChildView(menu_view_.get());
    menu_view_->SetSpeedButtonToggled(false);

    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
    bubble_view_->InitializeAndShowBubble();
  }

  // Update button states.
  menu_view_->SetPaused(is_paused);
  menu_view_->SetInitialSpeechRate(initial_speech_rate);

  // Add vertical spacing to given anchor rect.
  bubble_view_->ChangeAnchorRect(gfx::Rect(
      anchor.x(), anchor.y() - kAnchorRectVerticalSpacing, anchor.width(),
      anchor.height() + kAnchorRectVerticalSpacing * 2));

  if (!bubble_widget_->IsVisible()) {
    bubble_widget_->Show();
  }
  if (last_show_time_ == base::Time()) {
    last_show_time_ = base::Time::Now();
  }
}

void SelectToSpeakMenuBubbleController::Hide() {
  if (bubble_widget_) {
    bubble_widget_->Hide();
  }
  if (speed_bubble_controller_) {
    speed_bubble_controller_.reset();
  }
  MaybeRecordDurationHistogram();
}

void SelectToSpeakMenuBubbleController::MaybeRecordDurationHistogram() {
  if (last_show_time_ == base::Time()) {
    return;
  }
  base::UmaHistogramLongTimes100(
      "Accessibility.CrosSelectToSpeak.MenuBubbleVisibleDuration",
      base::Time::Now() - last_show_time_);
  last_show_time_ = base::Time();
}

std::u16string SelectToSpeakMenuBubbleController::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_MENU);
}

void SelectToSpeakMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

void SelectToSpeakMenuBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {
  // This function is currently not unused for bubbles of type
  // `kAccessibilityBubble`, so can leave this empty.
}

void SelectToSpeakMenuBubbleController::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active || !bubble_widget_)
    return;

  views::Widget* gained_widget =
      views::Widget::GetWidgetForNativeView(gained_active);
  if (gained_widget == bubble_widget_ && menu_view_ &&
      (!lost_active ||
       lost_active->GetName() != kSelectToSpeakSpeedBubbleWindowName)) {
    // Reset initial focus of the menu view, unless we're coming from the
    // reading speed selector.
    menu_view_->SetInitialFocus();
  }
}

void SelectToSpeakMenuBubbleController::OnActionSelected(
    SelectToSpeakPanelAction action) {
  if (action == SelectToSpeakPanelAction::kChangeSpeed) {
    // Toggle reading speed selection menu.
    if (!speed_bubble_controller_) {
      speed_bubble_controller_ =
          std::make_unique<SelectToSpeakSpeedBubbleController>(this);
    }
    if (speed_bubble_controller_->IsVisible()) {
      speed_bubble_controller_.reset();
      menu_view_->SetSpeedButtonToggled(false);
    } else {
      speed_bubble_controller_->Show(
          /*anchor=*/menu_view_, initial_speech_rate_);
      menu_view_->SetSpeedButtonToggled(true);
    }
    return;
  }
  Shell::Get()->accessibility_controller()->OnSelectToSpeakPanelAction(
      action, /*value=*/0.0);
}

void SelectToSpeakMenuBubbleController::OnSpeechRateSelected(
    double speech_rate) {
  if (speed_bubble_controller_) {
    menu_view_->SetSpeedButtonToggled(false);
    menu_view_->SetSpeedButtonFocused();
    speed_bubble_controller_->Hide();
    speed_bubble_controller_.reset();
  }
  Shell::Get()->accessibility_controller()->OnSelectToSpeakPanelAction(
      SelectToSpeakPanelAction::kChangeSpeed, /*value=*/speech_rate);
}

}  // namespace ash
