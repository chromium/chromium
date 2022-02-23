// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

using ArcDiskQuotaBridgeTest = testing::Test;
using PathType = user_data_auth::SetProjectIdAllowedPathType;

TEST(ArcDiskQuotaBridgeTest, ConvertPathForSetProjectId) {
  PathType parent_path;
  base::FilePath child_path;

  EXPECT_TRUE(ArcDiskQuotaBridge::convertPathForSetProjectId(
      base::FilePath("/storage/emulated/0/Download/mydir/test.png"),
      &parent_path, &child_path));
  EXPECT_EQ(PathType::PATH_DOWNLOADS, parent_path);
  EXPECT_EQ("mydir/test.png", child_path.value());

  EXPECT_TRUE(ArcDiskQuotaBridge::convertPathForSetProjectId(
      base::FilePath("/storage/emulated/0/Pictures/test.png"), &parent_path,
      &child_path));
  EXPECT_EQ(PathType::PATH_ANDROID_DATA, parent_path);
  EXPECT_EQ("data/media/0/Pictures/test.png", child_path.value());

  EXPECT_TRUE(ArcDiskQuotaBridge::convertPathForSetProjectId(
      base::FilePath("/data/media/0/Movies/test.mp4"), &parent_path,
      &child_path));
  EXPECT_EQ(PathType::PATH_ANDROID_DATA, parent_path);
  EXPECT_EQ("data/media/0/Movies/test.mp4", child_path.value());

  // Unallowed path.
  EXPECT_FALSE(ArcDiskQuotaBridge::convertPathForSetProjectId(
      base::FilePath("/storage/other/path"), &parent_path, &child_path));

  // Path contains ".."
  EXPECT_FALSE(ArcDiskQuotaBridge::convertPathForSetProjectId(
      base::FilePath("/data/media/0/../test.png"), &parent_path, &child_path));
}

}  // namespace
}  // namespace arc
