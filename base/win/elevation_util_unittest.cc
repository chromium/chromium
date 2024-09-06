// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/elevation_util.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_iterator.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace base::win {

namespace {

constexpr wchar_t kMoreExecutable[] = L"more.com";

bool IsExplorerRunningAtMediumOrLower() {
  ProcessId explorer_pid = GetExplorerPid();
  return explorer_pid ? IsProcessRunningAtMediumOrLower(explorer_pid) : false;
}

}  // namespace

TEST(ElevationUtil, RunDeElevated) {
  if (!::IsUserAnAdmin() || !IsExplorerRunningAtMediumOrLower()) {
    GTEST_SKIP();
  }

  Process process = RunDeElevated(CommandLine::FromString(L"more.com"));
  ASSERT_TRUE(process.IsValid());

  absl::Cleanup terminate_process = [&] {
    EXPECT_TRUE(process.Terminate(0, false));
  };

  ASSERT_TRUE(IsProcessRunningAtMediumOrLower(process.Pid()));
}

class ElevationUtilRunDeElevatedNoWaitTest
    : public ::testing::TestWithParam<RepeatingCallback<HRESULT()>> {};

INSTANTIATE_TEST_SUITE_P(ElevationUtilRunDeElevatedNoWaitTestCases,
                         ElevationUtilRunDeElevatedNoWaitTest,
                         ::testing::Values(BindRepeating([] {
                                             return RunDeElevatedNoWait(
                                                 CommandLine::FromString(
                                                     kMoreExecutable));
                                           }),
                                           BindRepeating([] {
                                             return RunDeElevatedNoWait(
                                                 kMoreExecutable, {});
                                           })));

TEST_P(ElevationUtilRunDeElevatedNoWaitTest, TestCases) {
  if (!::IsUserAnAdmin() || !IsExplorerRunningAtMediumOrLower()) {
    GTEST_SKIP();
  }

  ASSERT_EQ(GetProcessCount(kMoreExecutable, /*filter=*/nullptr), 0)
      << "This test requires that no instances of the `more` command are "
         "running.";

  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  ASSERT_TRUE(com_initializer.Succeeded());

  ASSERT_HRESULT_SUCCEEDED(GetParam().Run());

  // Wait for the process to start running.
  int i = 0;
  for (; i < 5; ++i) {
    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    if (GetProcessCount(kMoreExecutable, /*filter=*/nullptr) == 1) {
      break;
    }
  }
  ASSERT_LT(i, 5);

  NamedProcessIterator iter(kMoreExecutable, /*filter=*/nullptr);
  const ProcessEntry* process_entry = iter.NextProcessEntry();
  ASSERT_TRUE(process_entry);
  ASSERT_TRUE(IsProcessRunningAtMediumOrLower(process_entry->pid()));

  EXPECT_TRUE(Process::Open(process_entry->pid()).Terminate(0, false));
  ASSERT_FALSE(iter.NextProcessEntry());
}

}  // namespace base::win
