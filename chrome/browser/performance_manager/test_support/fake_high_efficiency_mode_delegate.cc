// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_high_efficiency_mode_delegate.h"

namespace performance_manager {

void FakeHighEfficiencyModeDelegate::ToggleHighEfficiencyMode(bool enabled) {}

void FakeHighEfficiencyModeDelegate::SetTimeBeforeDiscard(
    base::TimeDelta time_before_discard) {
  FakeHighEfficiencyModeDelegate::last_time_before_discard =
      time_before_discard;
}

base::TimeDelta FakeHighEfficiencyModeDelegate::GetLastTimeBeforeDiscard() {
  return last_time_before_discard;
}

}  // namespace performance_manager
