// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/stop_recording_button_tray.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

StopRecordingButtonTray::StopRecordingButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
}

StopRecordingButtonTray::~StopRecordingButtonTray() = default;

bool StopRecordingButtonTray::PerformAction(const ui::Event& event) {
  DCHECK(event.type() == ui::ET_MOUSE_RELEASED ||
         event.type() == ui::ET_GESTURE_TAP ||
         event.type() == ui::ET_KEY_PRESSED);

  base::RecordAction(base::UserMetricsAction("Tray_StopRecording"));
  CaptureModeController::Get()->EndVideoRecording(
      EndRecordingReason::kStopRecordingButton);
  return true;
}

std::u16string StopRecordingButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_AREA_STOP_RECORDING_BUTTON_ACCESSIBLE_NAME);
}

void StopRecordingButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  image_view_->SetImage(gfx::CreateVectorIcon(
      kCaptureModeCircleStopIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorAlert)));
}

}  // namespace ash
