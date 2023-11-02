// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_device_id_helper.h"

#include <string>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEpehemeralPrefix[] = "t_";

TEST(DeviceIdHelper, NotEphemeral) {
  std::string device_id =
      GenerateSigninScopedDeviceId(false /* for_ephemeral */);
  // Not empty.
  EXPECT_FALSE(device_id.empty());
  // No ephemeral prefix.
  EXPECT_FALSE(base::StartsWith(device_id, kEpehemeralPrefix,
                                base::CompareCase::SENSITIVE));
  // ID is unique.
  EXPECT_NE(device_id, GenerateSigninScopedDeviceId(false /* for_ephemeral */));
}

TEST(DeviceIdHelper, Ephemeral) {
  std::string device_id =
      GenerateSigninScopedDeviceId(true /* for_ephemeral */);
  // Ephemeral prefix.
  EXPECT_TRUE(base::StartsWith(device_id, kEpehemeralPrefix,
                               base::CompareCase::SENSITIVE));
  // Not empty.
  EXPECT_NE(device_id, kEpehemeralPrefix);
  // ID is unique.
  EXPECT_NE(device_id, GenerateSigninScopedDeviceId(true /* for_ephemeral */));
}
#endif
