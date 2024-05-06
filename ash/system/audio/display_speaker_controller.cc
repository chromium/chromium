// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/display_speaker_controller.h"

#include "ash/shell.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/util/display_util.h"

inline cras::DisplayRotation ToCRASDisplayRotation(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return cras::DisplayRotation::ROTATE_0;
    case display::Display::ROTATE_90:
      return cras::DisplayRotation::ROTATE_90;
    case display::Display::ROTATE_180:
      return cras::DisplayRotation::ROTATE_180;
    case display::Display::ROTATE_270:
      return cras::DisplayRotation::ROTATE_270;
  };
}

namespace ash {

DisplaySpeakerController::DisplaySpeakerController() {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

DisplaySpeakerController::~DisplaySpeakerController() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void DisplaySpeakerController::OnDisplayAdded(
    const display::Display& new_display) {
  if (!new_display.IsInternal())
    return;

  UpdateInternalSpeakerForDisplayRotation();

  // This event will be triggered when the lid of the device is opened to exit
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void DisplaySpeakerController::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    if (display.IsInternal()) {
      UpdateInternalSpeakerForDisplayRotation();

      // This event will be triggered when the lid of the device is closed to
      // enter the docked mode, we should always start or re-start HDMI
      // re-discovering grace period right after this event.
      CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(
          true);
      break;
    }
  }
}

void DisplaySpeakerController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!display.IsInternal())
    return;

  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) {
    UpdateInternalSpeakerForDisplayRotation();
  }

  // The event could be triggered multiple times during the HDMI display
  // transition, we don't need to restart HDMI re-discovering grace period
  // it is already started earlier.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(false);
}

void DisplaySpeakerController::SuspendDone(base::TimeDelta sleep_duration) {
  // This event is triggered when the device resumes after earlier suspension,
  // we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void DisplaySpeakerController::UpdateInternalSpeakerForDisplayRotation() {
  // Swap left/right channel only if it is in Yoga mode.
  bool swap = false;
  if (display::HasInternalDisplay()) {
    const display::ManagedDisplayInfo& display_info =
        Shell::Get()->display_manager()->GetDisplayInfo(
            display::Display::InternalDisplayId());
    display::Display::Rotation rotation = display_info.GetActiveRotation();
    if (rotation == display::Display::ROTATE_180)
      swap = true;

    CrasAudioHandler::Get()->SetDisplayRotation(
        ToCRASDisplayRotation(rotation));
  }
  CrasAudioHandler::Get()->SwapInternalSpeakerLeftRightChannel(swap);
}

}  // namespace ash
