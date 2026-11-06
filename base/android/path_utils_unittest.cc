// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/path_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

typedef testing::Test PathUtilsTest;

TEST_F(PathUtilsTest, TestGetDataDirectory) {
  // The string comes from the Java side and depends on the APK
  // we are running in. Assumes that we are packaged in
  // org.chromium.native_test
  FilePath path;
  GetDataDirectory(&path);

  // Looks for paths like
  // - /data/data/org.chromium.native_test/app_chrome
  // - /data/user/<user_id>/org.chromium.native_test/app_chrome
  EXPECT_THAT(
      path.value(),
      ::testing::MatchesRegex(
          R"(/data/(data|user/\d+)/org\.chromium\.native_test/app_chrome)"));
  EXPECT_TRUE(PathExists(path));
}

TEST_F(PathUtilsTest, TestGetCacheDirectory) {
  // The string comes from the Java side and depends on the APK
  // we are running in. Assumes that we are packaged in
  // org.chromium.native_test
  FilePath path;
  GetCacheDirectory(&path);

  // Looks for paths like
  // - /data/data/org.chromium.native_test/cache
  // - /data/user/<user_id>/org.chromium.native_test/cache
  EXPECT_THAT(path.value(),
              ::testing::MatchesRegex(
                  R"(/data/(data|user/\d+)/org\.chromium\.native_test/cache)"));
  EXPECT_TRUE(PathExists(path));
}

TEST_F(PathUtilsTest, TestGetNativeLibraryDirectory) {
  // The string comes from the Java side and depends on the APK
  // we are running in. Assumes that the directory contains
  // the base tests shared object.
  FilePath path;
  GetNativeLibraryDirectory(&path);
  EXPECT_NE(path.value().find("!/lib/"), std::string::npos)
      << "Expected !/lib/ in: " << path.value();
}

}  // namespace android
}  // namespace base
