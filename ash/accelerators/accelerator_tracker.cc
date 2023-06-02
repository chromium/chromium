// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_tracker.h"

#include <string>

#include "base/metrics/user_metrics.h"
#include "ui/events/event.h"

namespace ash {

AcceleratorTracker::AcceleratorTracker(
    base::span<const TrackerDataActionPair> tracker_data_list) {
  for (const auto& [tracker_data, user_action_name] : tracker_data_list) {
    accelerator_tracker_map_[tracker_data] = user_action_name;
  }
}

AcceleratorTracker::~AcceleratorTracker() = default;

void AcceleratorTracker::OnKeyEvent(ui::KeyEvent* event) {
  TrackerData trackerData(event->type() == ui::ET_KEY_PRESSED
                              ? KeyState::PRESSED
                              : KeyState::RELEASED,
                          event->key_code(), event->flags());
  const auto it = accelerator_tracker_map_.find(trackerData);
  if (it != accelerator_tracker_map_.end()) {
    base::RecordComputedAction(std::string(it->second));
  }
}

}  // namespace ash
