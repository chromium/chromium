// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/elevation_util.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace base::win {

namespace {

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

}  // namespace base::win
