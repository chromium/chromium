// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/disk_cache_dir_util.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

class DiskCacheDirUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterFilePathPref(prefs::kDiskCacheDir,
                                            base::FilePath());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    valid_dir_ = temp_dir_.GetPath().AppendASCII("valid_cache");
  }

  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir temp_dir_;
  base::FilePath valid_dir_;
};

TEST_F(DiskCacheDirUtilTest, NullLocalState) {
  EXPECT_EQ(GetDiskCacheDir(nullptr), base::FilePath());
}

TEST_F(DiskCacheDirUtilTest, UnsetPreference) {
  EXPECT_EQ(GetDiskCacheDir(&prefs_), base::FilePath());
}

TEST_F(DiskCacheDirUtilTest, UserControlledPreferenceRejected) {
  prefs_.SetFilePath(prefs::kDiskCacheDir, valid_dir_);
  EXPECT_EQ(GetDiskCacheDir(&prefs_), base::FilePath());
}

TEST_F(DiskCacheDirUtilTest, ManagedValidPreferenceStripsTrailingSeparator) {
  prefs_.SetManagedPref(
      prefs::kDiskCacheDir,
      base::FilePathToValue(valid_dir_.AsEndingWithSeparator()));
  EXPECT_EQ(GetDiskCacheDir(&prefs_), valid_dir_);
}

TEST_F(DiskCacheDirUtilTest, RelativePathRejected) {
  const base::FilePath relative_dir(FILE_PATH_LITERAL("relative/cache"));
  prefs_.SetManagedPref(prefs::kDiskCacheDir,
                        base::FilePathToValue(relative_dir));
  EXPECT_EQ(GetDiskCacheDir(&prefs_), base::FilePath());
}

TEST_F(DiskCacheDirUtilTest, ParentReferencingPathRejected) {
  const base::FilePath parent_ref_dir =
      valid_dir_.Append(base::FilePath::kParentDirectory);
  prefs_.SetManagedPref(prefs::kDiskCacheDir,
                        base::FilePathToValue(parent_ref_dir));
  EXPECT_EQ(GetDiskCacheDir(&prefs_), base::FilePath());
}

}  // namespace chrome_browser_net
