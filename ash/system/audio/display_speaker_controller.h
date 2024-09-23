// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_DISPLAY_SPEAKER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_DISPLAY_SPEAKER_CONTROLLER_H_

#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

namespace ash {

// Controller that does HDMI display audio and yoga mode handling.
class DisplaySpeakerController : public display::DisplayObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  DisplaySpeakerController();

  DisplaySpeakerController(const DisplaySpeakerController&) = delete;
  DisplaySpeakerController& operator=(const DisplaySpeakerController&) = delete;

  ~DisplaySpeakerController() override;

  // display::DisplayObserver.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

 private:
  // Update the state of internal speakers based on orientation.
  void UpdateInternalSpeakerForDisplayRotation();

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_DISPLAY_SPEAKER_CONTROLLER_H_
