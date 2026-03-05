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
#include "ash/system/tray/imaged_tray_icon.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

StopRecordingButtonTray::StopRecordingButtonTray(Shelf* shelf)
    : ImagedTrayIcon(
          shelf,
          ui::ImageModel::FromVectorIcon(kCaptureModeCircleStopIcon,
                                         kColorAshIconColorAlert),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_AREA_STOP_RECORDING_BUTTON_ACCESSIBLE_NAME),
          TrayBackgroundViewCatalogName::kScreenCaptureStopRecording) {
  SetCallback(base::BindRepeating([](const ui::Event& event) {
    base::RecordAction(base::UserMetricsAction("Tray_StopRecording"));
    CaptureModeController::Get()->EndVideoRecording(
        EndRecordingReason::kStopRecordingButton);
  }));

  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_AREA_STOP_RECORDING_BUTTON_ACCESSIBLE_NAME));
}

StopRecordingButtonTray::~StopRecordingButtonTray() = default;

BEGIN_METADATA(StopRecordingButtonTray)
END_METADATA

}  // namespace ash
