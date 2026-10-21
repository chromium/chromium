// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include <windows.h>

#include <processthreadsapi.h>

#include <optional>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool ServerCallback(int* callback_count,
                    base::CommandLine command_line,
                    const base::FilePath& current_directory) {
  ++(*callback_count);
  return true;
}

bool ClientCallback(base::CommandLine command_line,
                    const base::FilePath& current_directory) {
  ADD_FAILURE();
  return false;
}

}  // namespace

TEST(ChromeProcessSingletonTest, Basic) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  ChromeProcessSingleton ps1(profile_dir.GetPath());
  ps1.Unlock(
      base::BindRepeating(&ServerCallback, base::Unretained(&callback_count)));

  ChromeProcessSingleton ps2(profile_dir.GetPath());
  ps2.Unlock(base::BindRepeating(&ClientCallback));

  EXPECT_FALSE(ps1.IsSingletonInstanceForTesting());
  EXPECT_FALSE(ps2.IsSingletonInstanceForTesting());

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);

  EXPECT_TRUE(ps1.IsSingletonInstanceForTesting());
  EXPECT_FALSE(ps2.IsSingletonInstanceForTesting());

  ASSERT_EQ(1, callback_count);
}

TEST(ChromeProcessSingletonTest, Lock) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  ChromeProcessSingleton ps1(profile_dir.GetPath());

  ChromeProcessSingleton ps2(profile_dir.GetPath());
  ps2.Unlock(base::BindRepeating(&ClientCallback));

  EXPECT_FALSE(ps1.IsSingletonInstanceForTesting());
  EXPECT_FALSE(ps2.IsSingletonInstanceForTesting());

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);

  ASSERT_EQ(0, callback_count);
  ps1.Unlock(
      base::BindRepeating(&ServerCallback, base::Unretained(&callback_count)));
  ASSERT_EQ(1, callback_count);

  EXPECT_TRUE(ps1.IsSingletonInstanceForTesting());
  EXPECT_FALSE(ps2.IsSingletonInstanceForTesting());
}

// Only the singleton instance sets up direct launch, in StartWatching().
TEST(ChromeProcessSingletonTest, DirectLaunchOnlyInSingletonInstance) {
  // Recorded by every DirectLaunchManager as soon as it is created.
  static constexpr char kHistogram[] =
      "Startup.DirectLaunch.UserDataDirOverride";
  base::test::TaskEnvironment task_environment;
  // Keeps the DirectLaunchManager from calling into the shell.
  chrome::SetUsingDefaultUserDataDirectoryForTesting(false);
  base::ScopedClosureRunner reset_user_data_dir(
      base::BindOnce(&chrome::SetUsingDefaultUserDataDirectoryForTesting,
                     std::optional<bool>()));
  base::HistogramTester histogram_tester;
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  ChromeProcessSingleton ps1(profile_dir.GetPath());
  ChromeProcessSingleton ps2(profile_dir.GetPath());
  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, ps1.NotifyOtherProcessOrCreate());
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED,
            ps2.NotifyOtherProcessOrCreate());

  ps2.StartWatching();
  histogram_tester.ExpectTotalCount(kHistogram, 0);

  ps1.StartWatching();
  histogram_tester.ExpectUniqueSample(kHistogram, true, 1);

  ps2.Cleanup();
  ps1.Cleanup();
}

TEST(ChromeProcessSingletonTest, OverridePrefetch) {
  base::test::ScopedFeatureList scoped_feature(
      features::kOverridePrefetchOnSingleton);
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());
  ChromeProcessSingleton ps(profile_dir.GetPath());
  base::HistogramTester tester;

  ProcessSingleton::NotifyResult result = ps.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);

  ps.ChromeProcessSingleton::InitializeFeatures();

  auto buckets = tester.GetAllSamples("Startup.PrefetchOverrideErrorCode");
  EXPECT_EQ(1UL, buckets.size());
  EXPECT_EQ(1, buckets[0].count);
}
