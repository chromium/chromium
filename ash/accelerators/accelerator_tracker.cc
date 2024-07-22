// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_tracker.h"

#include <string>

#include "ash/capture_mode/capture_mode_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/events/event.h"

namespace ash {

AcceleratorTracker::AcceleratorTracker(
    base::span<const TrackerDataActionPair> tracker_data_list) {
  for (const auto& [tracker_data, metadata] : tracker_data_list) {
    accelerator_tracker_map_[tracker_data] = metadata;
  }
}

AcceleratorTracker::~AcceleratorTracker() = default;

void AcceleratorTracker::OnKeyEvent(ui::KeyEvent* event) {
  // We only care about specific modifiers, e.g., Caps Lock is irrelevant to us.
  constexpr ui::EventFlags kModifiersMask =
      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
      ui::EF_COMMAND_DOWN;
  const TrackerData tracker_data(
      event->type() == ui::EventType::kKeyPressed ? KeyState::PRESSED
                                                  : KeyState::RELEASED,
      event->key_code(), event->flags() & kModifiersMask);

  // If we find a match in the map:
  //  - Record it as a user action
  //  - If it is a screen-capture-related accelerator, then notify
  //    `CaptureModeEducationController` in case it would like to display
  //    a form of user education.
  const auto it = accelerator_tracker_map_.find(tracker_data);
  if (it != accelerator_tracker_map_.end()) {
    base::RecordComputedAction(std::string(it->second.action_string));
    if (it->second.type == TrackerType::kCaptureMode) {
      CaptureModeController::Get()
          ->education_controller()
          ->MaybeShowEducation();
    }
  }
}

}  // namespace ash
