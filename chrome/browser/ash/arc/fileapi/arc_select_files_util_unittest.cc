// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_select_files_util.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

TEST(ArcSelectFilesUtilTest, IsPickerPackageToExclude) {
  EXPECT_TRUE(IsPickerPackageToExclude("com.android.documentsui"));
  EXPECT_TRUE(IsPickerPackageToExclude("com.google.android.apps.docs"));
  EXPECT_FALSE(IsPickerPackageToExclude("com.google.photos"));
}

TEST(ArcSelectFilesUtilTest, ConvertAndroidActivityToFilePath) {
  EXPECT_EQ(
      "/special/android-activity/com.google.photos/.PickerActivity",
      ConvertAndroidActivityToFilePath("com.google.photos", ".PickerActivity")
          .value());

  // Invalid inputs.
  EXPECT_EQ("", ConvertAndroidActivityToFilePath(".", ".").value());
  EXPECT_EQ("", ConvertAndroidActivityToFilePath("..", "..").value());
  EXPECT_EQ("",
            ConvertAndroidActivityToFilePath("contains/slash", "contains/slash")
                .value());
}

TEST(ArcSelectFilesUtilTest, ConvertFilePathToAndroidActivity) {
  EXPECT_EQ(
      "com.google.photos/.PickerActivity",
      ConvertFilePathToAndroidActivity(base::FilePath(
          "/special/android-activity/com.google.photos/.PickerActivity")));

  EXPECT_EQ("", ConvertFilePathToAndroidActivity(
                    base::FilePath("/com.google.photos/.PickerActivity")));
}

}  // namespace

}  // namespace arc
