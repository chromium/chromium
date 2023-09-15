// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/file_system_util.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "chrome/test/base/testing_profile.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive::util {
namespace {

using ash::features::kDriveFsBulkPinning;
using ash::features::kFeatureManagementDriveFsBulkPinning;
using base::test::ScopedFeatureList;

// Marks the current thread as UI by BrowserTaskEnvironment. We need the task
// environment since Profile objects must be touched from UI and hence has
// CHECK/DCHECKs for it.
class ProfileRelatedFileSystemUtilTest : public testing::Test {
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

TEST_F(ProfileRelatedFileSystemUtilTest, SetDriveConnectionStatusForTesting) {
  TestingProfile profile;
  using enum ConnectionStatus;
  EXPECT_EQ(GetDriveConnectionStatus(&profile), kNoService);

  for (const ConnectionStatus status :
       {kNoNetwork, kNotReady, kNoService, kMetered, kConnected}) {
    SetDriveConnectionStatusForTesting(status);
    EXPECT_EQ(GetDriveConnectionStatus(&profile), status);
  }
}

TEST_F(ProfileRelatedFileSystemUtilTest, IsDriveFsBulkPinningAvailable) {
  TestingProfile profile;
  PrefService* const prefs = profile.GetPrefs();
  DCHECK(prefs);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kDriveFsBulkPinningVisible));

  {
    ScopedFeatureList features;
    features.InitWithFeatures(
        {kFeatureManagementDriveFsBulkPinning, kDriveFsBulkPinning}, {});
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable());
  }

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kFeatureManagementDriveFsBulkPinning},
                              {kDriveFsBulkPinning});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable());
  }

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kDriveFsBulkPinning},
                              {kFeatureManagementDriveFsBulkPinning});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable());
  }

  prefs->SetBoolean(prefs::kDriveFsBulkPinningVisible, false);

  {
    ScopedFeatureList features;
    features.InitWithFeatures(
        {kFeatureManagementDriveFsBulkPinning, kDriveFsBulkPinning}, {});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  }

  prefs->SetBoolean(prefs::kDriveFsBulkPinningVisible, true);

  {
    ScopedFeatureList features;
    features.InitWithFeatures(
        {kFeatureManagementDriveFsBulkPinning, kDriveFsBulkPinning}, {});
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  }
}

}  // namespace drive::util
