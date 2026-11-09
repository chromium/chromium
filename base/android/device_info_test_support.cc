// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/device_info_test_support.h"

#include <string>
#include <utility>

#include "base/android/device_info.h"
#include "base/types/pass_key.h"

namespace base::android::device_info {

ScopedDeviceName::ScopedDeviceName(std::string device_name)
    : previous_override_(
          set_device_name_for_testing(base::PassKey<ScopedDeviceName>(),
                                      std::move(device_name))) {}

ScopedDeviceName::~ScopedDeviceName() {
  set_device_name_for_testing(base::PassKey<ScopedDeviceName>(),
                              std::move(previous_override_));
}

}  // namespace base::android::device_info
