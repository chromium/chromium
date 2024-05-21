// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using WallpaperTypeTest = ::testing::Test;

TEST_F(WallpaperTypeTest, GetSupportedVersion) {
  {
    // WallpaperType::kOnline:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kOnline),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kOnline).IsValid());
  }
  {
    // WallpaperType::kDaily:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kDaily), base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kDaily).IsValid());
  }
  {
    // WallpaperType::kOnceGooglePhotos:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kOnceGooglePhotos),
              base::Version("1.0"));
    EXPECT_TRUE(
        GetSupportedVersion(WallpaperType::kOnceGooglePhotos).IsValid());
  }
  {  // WallpaperType::kDailyGooglePhotos:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kDailyGooglePhotos),
              base::Version("1.0"));
    EXPECT_TRUE(
        GetSupportedVersion(WallpaperType::kDailyGooglePhotos).IsValid());
  }
  {
    // WallpaperType::kCustomized:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kCustomized),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kCustomized).IsValid());
  }
  {
    // WallpaperType::kDefault:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kDefault),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kDefault).IsValid());
  }
  {
    // WallpaperType::kPolicy:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kPolicy),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kPolicy).IsValid());
  }
  {
    // WallpaperType::kThirdParty:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kThirdParty),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kThirdParty).IsValid());
  }
  {
    // WallpaperType::kDevice:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kDevice),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kDevice).IsValid());
  }
  {
    // WallpaperType::kOneShot:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kOneShot),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kOneShot).IsValid());
  }
  {
    // WallpaperType::kOobe:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kOobe), base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kOobe).IsValid());
  }
  {
    // WallpaperType::kSeaPen:
    EXPECT_EQ(GetSupportedVersion(WallpaperType::kSeaPen),
              base::Version("1.0"));
    EXPECT_TRUE(GetSupportedVersion(WallpaperType::kSeaPen).IsValid());
  }
  {
    // WallpaperType::kCount:
    EXPECT_FALSE(GetSupportedVersion(WallpaperType::kCount).IsValid());
  }
}

}  // namespace
}  // namespace ash
