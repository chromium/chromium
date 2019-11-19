// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/display_speaker_controller.h"

#include "ash/shell.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"

using chromeos::CrasAudioHandler;

namespace ash {

DisplaySpeakerController::DisplaySpeakerController() {
  display::Screen::GetScreen()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

DisplaySpeakerController::~DisplaySpeakerController() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

void DisplaySpeakerController::OnDisplayAdded(
    const display::Display& new_display) {
  if (!new_display.IsInternal())
    return;
  ChangeInternalSpeakerChannelMode();

  // This event will be triggered when the lid of the device is opened to exit
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void DisplaySpeakerController::OnDisplayRemoved(
    const display::Display& old_display) {
  if (!old_display.IsInternal())
    return;
  ChangeInternalSpeakerChannelMode();

  // This event will be triggered when the lid of the device is closed to enter
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void DisplaySpeakerController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!display.IsInternal())
    return;

  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)
    ChangeInternalSpeakerChannelMode();

  // The event could be triggered multiple times during the HDMI display
  // transition, we don't need to restart HDMI re-discovering grace period
  // it is already started earlier.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(false);
}

void DisplaySpeakerController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // This event is triggered when the device resumes after earlier suspension,
  // we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void DisplaySpeakerController::ChangeInternalSpeakerChannelMode() {
  // Swap left/right channel only if it is in Yoga mode.
  bool swap = false;
  if (display::Display::HasInternalDisplay()) {
    const display::ManagedDisplayInfo& display_info =
        Shell::Get()->display_manager()->GetDisplayInfo(
            display::Display::InternalDisplayId());
    if (display_info.GetActiveRotation() == display::Display::ROTATE_180)
      swap = true;
  }
  CrasAudioHandler::Get()->SwapInternalSpeakerLeftRightChannel(swap);
}

}  // namespace ash
