// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_DISPLAY_SPEAKER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_DISPLAY_SPEAKER_CONTROLLER_H_

#include "base/macros.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/display_observer.h"

namespace ash {

// Controller that does HDMI display audio and yoga mode handling.
class DisplaySpeakerController : public display::DisplayObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  DisplaySpeakerController();
  ~DisplaySpeakerController() override;

  // display::DisplayObserver.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

 private:
  // Swaps the left and right channels on yoga devices based on orientation.
  void ChangeInternalSpeakerChannelMode();

  DISALLOW_COPY_AND_ASSIGN(DisplaySpeakerController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_DISPLAY_SPEAKER_CONTROLLER_H_
