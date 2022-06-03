// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/hotseat_state_watcher.h"

namespace ash {

HotseatStateWatcher::HotseatStateWatcher(
    ShelfLayoutManager* shelf_layout_manager)
    : shelf_layout_manager_(shelf_layout_manager) {
  shelf_layout_manager_->AddObserver(this);
}

HotseatStateWatcher::~HotseatStateWatcher() {
  shelf_layout_manager_->RemoveObserver(this);
}

void HotseatStateWatcher::OnHotseatStateChanged(HotseatState old_state,
                                                HotseatState new_state) {
  run_loop_.QuitWhenIdle();
  state_changes_.push_back(new_state);
}

void HotseatStateWatcher::CheckEqual(std::vector<HotseatState> state_changes) {
  EXPECT_EQ(state_changes_, state_changes);
}

void HotseatStateWatcher::WaitUntilStateChanged() {
  run_loop_.Run();
}

}  // namespace ash
