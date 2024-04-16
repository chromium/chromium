// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_freezing_delegate.h"

namespace performance_manager {

void FakeFreezingDelegate::ToggleFreezingOnBatterySaverMode(bool is_enabled) {
  *freezing_on_battery_saver_enabled_ = is_enabled;
}

FakeFreezingDelegate::FakeFreezingDelegate(
    bool* freezing_on_battery_saver_enabled)
    : freezing_on_battery_saver_enabled_(freezing_on_battery_saver_enabled) {}

}  // namespace performance_manager
