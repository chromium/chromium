// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_memory_saver_mode_delegate.h"

namespace performance_manager::user_tuning {

void FakeMemorySaverModeDelegate::ToggleMemorySaverMode(
    prefs::MemorySaverModeState state) {
  last_state_ = state;
}

void FakeMemorySaverModeDelegate::SetTimeBeforeDiscard(
    base::TimeDelta time_before_discard) {
  last_time_before_discard_ = time_before_discard;
}

void FakeMemorySaverModeDelegate::ClearLastState() {
  last_state_.reset();
}

std::optional<prefs::MemorySaverModeState>
FakeMemorySaverModeDelegate::GetLastState() const {
  return last_state_;
}

std::optional<base::TimeDelta>
FakeMemorySaverModeDelegate::GetLastTimeBeforeDiscard() const {
  return last_time_before_discard_;
}

}  // namespace performance_manager::user_tuning
