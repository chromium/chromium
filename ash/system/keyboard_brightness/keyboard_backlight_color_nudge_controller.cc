// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_nudge_controller.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/controls/contextual_nudge.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace {

const int kEducationBubblePreferredWidth = 300;

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

KeyboardBacklightColorNudgeController::KeyboardBacklightColorNudgeController() =
    default;

KeyboardBacklightColorNudgeController::
    ~KeyboardBacklightColorNudgeController() = default;

// static
bool KeyboardBacklightColorNudgeController::ShouldShowWallpaperColorNudge() {
  return contextual_tooltip::ShouldShowNudge(
      GetActivePrefService(),
      contextual_tooltip::TooltipType::kKeyboardBacklightWallpaperColor,
      /*recheck_delay=*/nullptr);
}

// static
void KeyboardBacklightColorNudgeController::HandleWallpaperColorNudgeShown() {
  HandleNudgeShown(
      GetActivePrefService(),
      contextual_tooltip::TooltipType::kKeyboardBacklightWallpaperColor);
}

void KeyboardBacklightColorNudgeController::MaybeShowEducationNudge(
    views::View* keyboard_brightness_slider_view) {
  if (!keyboard_brightness_slider_view || education_nudge_) {
    return;
  }
  if (!contextual_tooltip::ShouldShowNudge(
          GetActivePrefService(),
          contextual_tooltip::TooltipType::kKeyboardBacklightColor,
          /*recheck_delay=*/nullptr)) {
    return;
  }

  // The nudge is anchored on the top right of
  // |keyboard_brightness_slider_view|. It owns itself and destructed when
  // closed.
  education_nudge_ = new ContextualNudge(
      /*anchor=*/nullptr,
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_SettingBubbleContainer),
      ContextualNudge::Position::kTop, gfx::Insets(16),
      l10n_util::GetStringFUTF16(
          IDS_ASH_KEYBOARD_BACKLIGHT_COLOR_EDUCATION_NUDGE_TEXT,
          l10n_util::GetStringUTF16(
              IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE)),
      base::BindRepeating(
          &KeyboardBacklightColorNudgeController::CloseEducationNudge,
          weak_factory_.GetWeakPtr()));

  views::Label* label = education_nudge_->label();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SizeToFit(kEducationBubblePreferredWidth);
  education_nudge_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);

  ui::Layer* layer = education_nudge_->layer();
  layer->SetColor(
      ShelfConfig::Get()->GetDefaultShelfColor(education_nudge_->GetWidget()));
  layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF{static_cast<float>(GetBubbleCornerRadius())});
  layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  gfx::Rect anchor_rect =
      keyboard_brightness_slider_view->GetAnchorBoundsInScreen();
  // Update the anchor rect and add a vertical gap between the anchor view and
  // nudge.
  education_nudge_->UpdateAnchorRect(
      gfx::Rect(anchor_rect.x(), anchor_rect.y() - 8, anchor_rect.width(),
                anchor_rect.height()));
  education_nudge_->SetArrow(views::BubbleBorder::BOTTOM_RIGHT);
  education_nudge_->GetWidget()->Show();

  contextual_tooltip::HandleNudgeShown(
      GetActivePrefService(),
      contextual_tooltip::TooltipType::kKeyboardBacklightColor);

  StartAutoCloseTimer();
}

void KeyboardBacklightColorNudgeController::CloseEducationNudge() {
  autoclose_.Stop();
  if (!education_nudge_ || education_nudge_->GetWidget()->IsClosed()) {
    return;
  }
  education_nudge_->GetWidget()->Close();
  education_nudge_ = nullptr;
}

void KeyboardBacklightColorNudgeController::SetUserPerformedAction() {
  // Indicate the user has selected a color so we won't show the education nudge
  // anymore.
  contextual_tooltip::HandleGesturePerformed(
      Shell::Get()->session_controller()->GetActivePrefService(),
      contextual_tooltip::TooltipType::kKeyboardBacklightColor);
}

void KeyboardBacklightColorNudgeController::StartAutoCloseTimer() {
  autoclose_.Stop();
  base::TimeDelta nudge_duration = contextual_tooltip::GetNudgeTimeout(
      GetActivePrefService(),
      contextual_tooltip::TooltipType::kKeyboardBacklightColor);
  if (!nudge_duration.is_zero()) {
    autoclose_.Start(
        FROM_HERE, nudge_duration, this,
        &KeyboardBacklightColorNudgeController::CloseEducationNudge);
  } else {
    CloseEducationNudge();
  }
}

}  // namespace ash
