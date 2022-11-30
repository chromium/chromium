// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_main_delegate_android.h"

#include <memory>

#include "base/base_paths_android.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeMainDelegateAndroidTest : public testing::Test {
 protected:
  ChromeMainDelegateAndroidTest() {}
  ~ChromeMainDelegateAndroidTest() override {}

  void SetUp() override {
    ASSERT_TRUE(mock_data_dir_.CreateUniqueTempDir());
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_ANDROID_APP_DATA, mock_data_dir_.GetPath());
  }

  base::FilePath dataDir() const { return mock_data_dir_.GetPath(); }

 private:
  base::ScopedTempDir mock_data_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
};

TEST_F(ChromeMainDelegateAndroidTest, VerifyDataDirPermissions) {
  EXPECT_TRUE(base::SetPosixFilePermissions(dataDir(), 0777));
  ChromeMainDelegateAndroid::SecureDataDirectory();
  int new_mode;
  EXPECT_TRUE(base::GetPosixFilePermissions(dataDir(), &new_mode));
  EXPECT_EQ(new_mode, 0700);
}
