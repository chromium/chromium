// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/stop_recording_button_tray.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

StopRecordingButtonTray::StopRecordingButtonTray(Shelf* shelf)
    : TrayBackgroundView(
          shelf,
          TrayBackgroundViewCatalogName::kScreenCaptureStopRecording,
          RoundedCornerBehavior::kAllRounded),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  SetCallback(base::BindRepeating([](const ui::Event& event) {
    base::RecordAction(base::UserMetricsAction("Tray_StopRecording"));
    CaptureModeController::Get()->EndVideoRecording(
        EndRecordingReason::kStopRecordingButton);
  }));
  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
}

StopRecordingButtonTray::~StopRecordingButtonTray() = default;

std::u16string StopRecordingButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_AREA_STOP_RECORDING_BUTTON_ACCESSIBLE_NAME);
}

void StopRecordingButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  image_view_->SetImage(gfx::CreateVectorIcon(
      kCaptureModeCircleStopIcon,
      GetColorProvider()->GetColor(kColorAshIconColorAlert)));
}

BEGIN_METADATA(StopRecordingButtonTray)
END_METADATA

}  // namespace ash
