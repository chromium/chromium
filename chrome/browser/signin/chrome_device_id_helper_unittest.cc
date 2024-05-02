// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_device_id_helper.h"

#include <string>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/signin_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using testing::Eq;
using testing::IsEmpty;
using testing::Ne;
using testing::Not;
using testing::StartsWith;

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEpehemeralPrefix[] = "t_";

TEST(DeviceIdHelper, NonEphemeralDeviceIdsAreNotEmpty) {
  EXPECT_THAT(GenerateSigninScopedDeviceId(/*for_ephemeral=*/false),
              Not(IsEmpty()));
}

TEST(DeviceIdHelper, NonEphemeralDeviceIdsDoNotHaveTheEphemeralPrefix) {
  EXPECT_THAT(GenerateSigninScopedDeviceId(/*for_ephemeral=*/false),
              Not(StartsWith(kEpehemeralPrefix)));
}

TEST(DeviceIdHelper,
     NonEphemeralDeviceIdsAreUniqueIfStableDeviceIdFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  // Newly generated id is not the same as the previous one.
  EXPECT_THAT(device_id2, Ne(device_id1));
}

TEST(DeviceIdHelper,
     NonEphemeralDeviceIdsAreNotUniqueIfStableDeviceIdFeatureIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  // Newly generated id is the same as the previous one.
  EXPECT_THAT(device_id2, Eq(device_id1));
}

TEST(DeviceIdHelper, EphemeralDeviceIdsAreNotEmpty) {
  EXPECT_THAT(GenerateSigninScopedDeviceId(/*for_ephemeral=*/true),
              Not(IsEmpty()));
}

TEST(DeviceIdHelper, EphemeralDeviceIdsHaveTheEphemeralPrefix) {
  EXPECT_THAT(GenerateSigninScopedDeviceId(/*for_ephemeral=*/true),
              StartsWith(kEpehemeralPrefix));
}

TEST(DeviceIdHelper,
     EphemeralDeviceIdsAreUniqueIfStableDeviceIdFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  // Newly generated id is not the same as the previous one.
  EXPECT_THAT(device_id2, Ne(device_id1));
}

TEST(DeviceIdHelper,
     EphemeralDeviceIdsAreUniqueIfStableDeviceIdFeatureIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  // Newly generated id is not the same as the previous one.
  EXPECT_THAT(device_id2, Ne(device_id1));
}

#endif
