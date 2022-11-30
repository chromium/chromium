// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_lock_screen_instance.h"

namespace arc {

FakeLockScreenInstance::FakeLockScreenInstance() = default;
FakeLockScreenInstance::~FakeLockScreenInstance() = default;

void FakeLockScreenInstance::SetDeviceLocked(bool is_locked) {
  is_locked_ = is_locked;
}

}  // namespace arc
