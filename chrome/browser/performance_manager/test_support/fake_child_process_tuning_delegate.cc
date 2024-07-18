// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_child_process_tuning_delegate.h"

namespace performance_manager {

FakeChildProcessTuningDelegate::FakeChildProcessTuningDelegate(
    bool* child_process_tuning_enabled)
    : child_process_tuning_enabled_(child_process_tuning_enabled) {}

FakeChildProcessTuningDelegate::~FakeChildProcessTuningDelegate() = default;

void FakeChildProcessTuningDelegate::SetBatterySaverModeForAllChildProcessHosts(
    bool enabled) {
  *child_process_tuning_enabled_ = enabled;
}

}  // namespace performance_manager
