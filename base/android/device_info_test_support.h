// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_DEVICE_INFO_TEST_SUPPORT_H_
#define BASE_ANDROID_DEVICE_INFO_TEST_SUPPORT_H_

#include <optional>
#include <string>

namespace base::android::device_info {

// Overrides the value returned by `device_name()` for the lifetime of this
// object, restoring the previous state upon destruction. Marked `[[nodiscard]]`
// to prevent callers from accidentally creating an unnamed temporary that is
// destroyed immediately.
class [[nodiscard]] ScopedDeviceName {
 public:
  explicit ScopedDeviceName(std::string device_name);
  ScopedDeviceName(const ScopedDeviceName&) = delete;
  ScopedDeviceName& operator=(const ScopedDeviceName&) = delete;
  ~ScopedDeviceName();

 private:
  std::optional<std::string> previous_override_;
};

}  // namespace base::android::device_info

#endif  // BASE_ANDROID_DEVICE_INFO_TEST_SUPPORT_H_
