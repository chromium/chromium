// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/run_loop.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "testing/multiprocess_func_list.h"

namespace base {
namespace android {

MULTIPROCESS_TEST_MAIN(BasicMain) {
  return 0;
}

MULTIPROCESS_TEST_MAIN(WaitingMain) {
  base::RunLoop().Run();
  return 0;
}

class ChildProcessTest : public MultiProcessTest {};

// Test disabled due to flakiness: https://crbug.com/950772.
TEST_F(ChildProcessTest, DISABLED_ChildHasCleanExit) {
  Process process = SpawnChild("BasicMain");
  int exit_code = 0;
  EXPECT_TRUE(WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(exit_code, 0);
  EXPECT_TRUE(MultiProcessTestChildHasCleanExit(process));
}

TEST_F(ChildProcessTest, ChildTerminated) {
  Process process = SpawnChild("WaitingMain");
  EXPECT_TRUE(TerminateMultiProcessTestChild(process, 0, true));
  EXPECT_FALSE(MultiProcessTestChildHasCleanExit(process));
}

}  // namespace android
}  // namespace base
