// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/device_info.h"

#include <string>

#include "base/android/device_info_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::android::device_info {

TEST(DeviceInfoTest, ScopedDeviceName) {
  const std::string original_device_name = device_name();

  {
    ScopedDeviceName outer_override("Outer Test Device");
    EXPECT_EQ("Outer Test Device", device_name());

    {
      ScopedDeviceName inner_override("Inner Test Device");
      EXPECT_EQ("Inner Test Device", device_name());
    }

    EXPECT_EQ("Outer Test Device", device_name());

    {
      ScopedDeviceName empty_override("");
      EXPECT_EQ("", device_name());

      {
        ScopedDeviceName nested_override("Nested After Empty");
        EXPECT_EQ("Nested After Empty", device_name());
      }

      EXPECT_EQ("", device_name());
    }

    EXPECT_EQ("Outer Test Device", device_name());
  }

  EXPECT_EQ(original_device_name, device_name());
}

}  // namespace base::android::device_info
