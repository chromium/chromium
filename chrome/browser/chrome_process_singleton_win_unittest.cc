// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
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
