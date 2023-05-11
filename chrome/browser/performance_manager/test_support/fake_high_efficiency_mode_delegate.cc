// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_high_efficiency_mode_delegate.h"

namespace performance_manager::user_tuning {

void FakeHighEfficiencyModeDelegate::ToggleHighEfficiencyMode(
    prefs::HighEfficiencyModeState state) {
  last_state_ = state;
}

void FakeHighEfficiencyModeDelegate::SetTimeBeforeDiscard(
    base::TimeDelta time_before_discard) {
  last_time_before_discard_ = time_before_discard;
}

void FakeHighEfficiencyModeDelegate::ClearLastState() {
  last_state_.reset();
}

absl::optional<prefs::HighEfficiencyModeState>
FakeHighEfficiencyModeDelegate::GetLastState() const {
  return last_state_;
}

absl::optional<base::TimeDelta>
FakeHighEfficiencyModeDelegate::GetLastTimeBeforeDiscard() const {
  return last_time_before_discard_;
}

}  // namespace performance_manager::user_tuning
