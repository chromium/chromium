// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/file_system_util.h"

#include "base/files/file_path.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace util {

namespace {

// Marks the current thread as UI by BrowserTaskEnvironment. We need the task
// environment since Profile objects must be touched from UI and hence has
// CHECK/DCHECKs for it.
class ProfileRelatedFileSystemUtilTest : public testing::Test {
 protected:
  ProfileRelatedFileSystemUtilTest() {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

}  // namespace

TEST_F(ProfileRelatedFileSystemUtilTest, IsUnderDriveMountPoint) {
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("media/fuse/drivefs/foo.txt")));

  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs/subdir/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs-xxx/foo.txt")));
}

TEST_F(ProfileRelatedFileSystemUtilTest, GetCacheRootPath) {
  TestingProfile profile;
  base::FilePath profile_path = profile.GetPath();
  EXPECT_EQ(profile_path.AppendASCII("GCache/v1"),
            util::GetCacheRootPath(&profile));
}

}  // namespace util
}  // namespace drive
