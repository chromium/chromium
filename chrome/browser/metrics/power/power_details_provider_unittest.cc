// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_details_provider.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MAC)
TEST(BatteryLevelProviderTest, Brightness) {
  auto provider = PowerDetailsProvider::Create();
  // There isn't much to test as test bots are usually not using a display.
  EXPECT_NO_FATAL_FAILURE(provider->GetMainScreenBrightnessLevel());
}
#endif
