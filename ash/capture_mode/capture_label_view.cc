// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_label_view.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

bool g_use_delay_for_testing = false;

// Capture label button rounded corner radius.
constexpr int kCaptureLabelRadius = 18;

constexpr base::TimeDelta kCountDownDuration = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kCountDownDurationForTesting =
    base::TimeDelta::FromMilliseconds(10);

}  // namespace

CaptureLabelView::CaptureLabelView(CaptureModeSession* capture_mode_session)
    : capture_mode_session_(capture_mode_session) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  SetBackground(views::CreateSolidBackground(background_color));
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCaptureLabelRadius));
  layer()->SetBackgroundBlur(
      static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));

  SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  label_button_ = AddChildView(
      std::make_unique<views::LabelButton>(this, base::string16()));
  label_button_->SetPaintToLayer();
  label_button_->layer()->SetFillsBoundsOpaquely(false);
  label_button_->SetEnabledTextColors(text_color);
  label_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_button_->SetNotifyEnterExitOnChild(true);

  label_button_->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  const auto ripple_attributes =
      color_provider->GetRippleAttributes(background_color);
  label_button_->SetInkDropVisibleOpacity(ripple_attributes.inkdrop_opacity);
  label_button_->SetInkDropBaseColor(ripple_attributes.base_color);

  label_ = AddChildView(std::make_unique<views::Label>(base::string16()));
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetEnabledColor(text_color);
  label_->SetBackgroundColor(SK_ColorTRANSPARENT);

  UpdateIconAndText();
}

CaptureLabelView::~CaptureLabelView() = default;

// static
void CaptureLabelView::SetUseDelayForTesting(bool use_delay) {
  g_use_delay_for_testing = true;
}

void CaptureLabelView::UpdateIconAndText() {
  CaptureModeController* controller = CaptureModeController::Get();
  const CaptureModeSource source = controller->source();
  const bool is_capturing_image = controller->type() == CaptureModeType::kImage;
  const bool in_tablet_mode = TabletModeController::Get()->InTabletMode();
  auto* color_provider = AshColorProvider::Get();
  SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  gfx::ImageSkia icon;
  base::string16 text;
  switch (source) {
    case CaptureModeSource::kFullscreen:
      icon = is_capturing_image
                 ? gfx::ImageSkia()
                 : gfx::CreateVectorIcon(kCaptureModeVideoIcon, icon_color);
      text = l10n_util::GetStringUTF16(
          is_capturing_image
              ? (in_tablet_mode
                     ? IDS_ASH_SCREEN_CAPTURE_LABEL_FULLSCREEN_IMAGE_CAPTURE_TABLET
                     : IDS_ASH_SCREEN_CAPTURE_LABEL_FULLSCREEN_IMAGE_CAPTURE_CLAMSHELL)
              : IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD);
      break;
    case CaptureModeSource::kWindow: {
      if (in_tablet_mode) {
        text = l10n_util::GetStringUTF16(
            is_capturing_image
                ? IDS_ASH_SCREEN_CAPTURE_LABEL_WINDOW_IMAGE_CAPTURE
                : IDS_ASH_SCREEN_CAPTURE_LABEL_WINDOW_VIDEO_RECORD);
      }
      break;
    }
    case CaptureModeSource::kRegion: {
      if (!capture_mode_session_->is_selecting_region()) {
        if (CaptureModeController::Get()->user_capture_region().IsEmpty()) {
          // We're now in waiting to select a capture region phase.
          text = l10n_util::GetStringUTF16(
              is_capturing_image
                  ? IDS_ASH_SCREEN_CAPTURE_LABEL_REGION_IMAGE_CAPTURE
                  : IDS_ASH_SCREEN_CAPTURE_LABEL_REGION_VIDEO_RECORD);
        } else {
          // We're now in fine-tuning phase.
          icon = is_capturing_image
                     ? gfx::CreateVectorIcon(kCaptureModeImageIcon, icon_color)
                     : gfx::CreateVectorIcon(kCaptureModeVideoIcon, icon_color);
          text = l10n_util::GetStringUTF16(
              is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_LABEL_IMAGE_CAPTURE
                                 : IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD);
        }
      }
      break;
    }
  }

  if (!icon.isNull()) {
    label_->SetVisible(false);
    label_button_->SetVisible(true);
    label_button_->SetImage(views::Button::STATE_NORMAL, icon);
    label_button_->SetText(text);
  } else if (!text.empty()) {
    label_button_->SetVisible(false);
    label_->SetVisible(true);
    label_->SetText(text);
  } else {
    label_button_->SetVisible(false);
    label_->SetVisible(false);
  }
}

bool CaptureLabelView::ShouldHandleEvent() {
  return label_button_->GetVisible();
}

void CaptureLabelView::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  countdown_finished_callback_ = std::move(countdown_finished_callback);
  label_button_->SetVisible(false);
  label_->SetVisible(true);

  CountDown();
  base::TimeDelta duration = g_use_delay_for_testing
                                 ? kCountDownDurationForTesting
                                 : kCountDownDuration;
  count_down_timer_.Start(FROM_HERE, duration, this,
                          &CaptureLabelView::CountDown);
}

void CaptureLabelView::Layout() {
  label_button_->SetBoundsRect(GetLocalBounds());

  gfx::Rect label_bounds = GetLocalBounds();
  label_bounds.ClampToCenteredSize(label_->GetPreferredSize());
  label_->SetBoundsRect(label_bounds);
}

gfx::Size CaptureLabelView::CalculatePreferredSize() const {
  if (count_down_timer_.IsRunning())
    return gfx::Size(kCaptureLabelRadius * 2, kCaptureLabelRadius * 2);

  const bool is_label_button_visible = label_button_->GetVisible();
  const bool is_label_visible = label_->GetVisible();

  if (!is_label_button_visible && !is_label_visible)
    return gfx::Size();

  if (is_label_button_visible) {
    DCHECK(!is_label_visible);
    return gfx::Size(
        label_button_->GetPreferredSize().width() + kCaptureLabelRadius * 2,
        kCaptureLabelRadius * 2);
  }

  DCHECK(is_label_visible && !is_label_button_visible);
  return gfx::Size(label_->GetPreferredSize().width() + kCaptureLabelRadius * 2,
                   kCaptureLabelRadius * 2);
}

void CaptureLabelView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK_EQ(static_cast<views::Button*>(label_button_), sender);
  CaptureModeController::Get()->PerformCapture();
}

void CaptureLabelView::CountDown() {
  if (timeout_count_down_ == 0) {
    std::move(countdown_finished_callback_).Run();  // |this| is destroyed here.
    return;
  }

  label_->SetText(base::FormatNumber(timeout_count_down_--));
}

BEGIN_METADATA(CaptureLabelView, views::View)
END_METADATA

}  // namespace ash
