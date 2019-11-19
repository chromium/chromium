// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace first_run {

class FirstRunTest : public testing::Test {
 protected:
  FirstRunTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}
  ~FirstRunTest() override {}

  void TearDown() override {
    first_run::ResetCachedSentinelDataForTesting();
    Test::TearDown();
  }

 private:
  base::ScopedPathOverride user_data_dir_override_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunTest);
};

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_NoVariationsSeed) {
  installer::MasterPreferences install_prefs("{ }");
  EXPECT_TRUE(install_prefs.master_dictionary().empty());

  EXPECT_TRUE(install_prefs.GetCompressedVariationsSeed().empty());
  EXPECT_TRUE(install_prefs.GetVariationsSeedSignature().empty());
}

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_VariationsSeedSignature) {
  installer::MasterPreferences install_prefs(
      "{\"variations_compressed_seed\":\"xyz\","
      " \"variations_seed_signature\":\"abc\"}");
  EXPECT_EQ(2U, install_prefs.master_dictionary().size());

  EXPECT_EQ("xyz", install_prefs.GetCompressedVariationsSeed());
  EXPECT_EQ("abc", install_prefs.GetVariationsSeedSignature());
  // Variations prefs should have been extracted (removed) from the dictionary.
  EXPECT_TRUE(install_prefs.master_dictionary().empty());
}

// No switches and no sentinel present. This is the standard case for first run.
TEST_F(FirstRunTest, DetermineFirstRunState_FirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// Force switch is present, overriding both sentinel and suppress switch.
TEST_F(FirstRunTest, DetermineFirstRunState_ForceSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(true, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// No switches, but sentinel present. This is not a first run.
TEST_F(FirstRunTest, DetermineFirstRunState_NotFirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, false, false);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

// Suppress switch is present, overriding sentinel state.
TEST_F(FirstRunTest, DetermineFirstRunState_SuppressSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);

  result = internal::DetermineFirstRunState(true, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

TEST_F(FirstRunTest, GetFirstRunSentinelCreationTime_Created) {
  first_run::CreateSentinelIfNeeded();
  // Gets the creation time of the first run sentinel.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(user_data_dir.Append(chrome::kFirstRunSentinel),
                                &info));

  EXPECT_EQ(info.creation_time, first_run::GetFirstRunSentinelCreationTime());
}

TEST_F(FirstRunTest, GetFirstRunSentinelCreationTime_NotCreated) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::File::Info info;
  ASSERT_FALSE(base::GetFileInfo(
      user_data_dir.Append(chrome::kFirstRunSentinel), &info));

  EXPECT_EQ(0, first_run::GetFirstRunSentinelCreationTime().ToDoubleT());
}

}  // namespace first_run
