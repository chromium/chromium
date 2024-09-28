// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/asan_service.h"

#if defined(ADDRESS_SANITIZER)

#include <map>
#include <memory>
#include <sstream>

#include "base/debug/asan_invalid_access.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// All of the tests require death tests, so there's nothing to build if we're
// building for a platform that doesn't support them.
#if defined(GTEST_HAS_DEATH_TEST)

namespace base {
namespace debug {

class AsanServiceTest : public ::testing::Test {
 protected:
  void SetUp() override { AsanService::GetInstance()->Initialize(); }
};

bool ExitedCleanly(int exit_status) {
  return exit_status == 0;
}

// TODO(crbug.com/40884672): ASAN death test is not picking up the failure
// in the emulator logs. Disabling to keep ASAN queue clear.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ErrorCallback DISABLED_ErrorCallback
#define MAYBE_CrashInErrorCallback DISABLED_CrashInErrorCallback
#define MAYBE_ShouldExitCleanly DISABLED_ShouldExitCleanly
#define MAYBE_TaskTraceCallback DISABLED_TaskTraceCallback
#else
#define MAYBE_ErrorCallback ErrorCallback
#define MAYBE_CrashInErrorCallback CrashInErrorCallback
#define MAYBE_ShouldExitCleanly ShouldExitCleanly
#define MAYBE_TaskTraceCallback TaskTraceCallback
#endif

TEST_F(AsanServiceTest, MAYBE_ErrorCallback) {
  // Register an error callback, and check that the output is added.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback1");
  });
  EXPECT_DEATH(AsanHeapUseAfterFree(), "ErrorCallback1");

  // Register a second error callback, and check that the output from both
  // callbacks is added.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback2");
  });
  EXPECT_DEATH(AsanHeapUseAfterFree(), "ErrorCallback1");
  EXPECT_DEATH(AsanHeapUseAfterFree(), "ErrorCallback2");
}

TEST_F(AsanServiceTest, MAYBE_CrashInErrorCallback) {
  // If a nested fault happens, we don't expect to get our custom log messages
  // displayed, but we should still get some part of the ASan report. This
  // matches current ASan recursive fault handling - make sure we don't end up
  // deadlocking.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback1");
    AsanHeapUseAfterFree();
  });
  EXPECT_DEATH(AsanHeapUseAfterFree(),
               "AddressSanitizer: nested bug in the same thread");
}

TEST_F(AsanServiceTest, MAYBE_ShouldExitCleanly) {
  // Register an error callback, and check that the output is added.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback1");
  });
  EXPECT_DEATH(AsanHeapUseAfterFree(), "ErrorCallback1");
  EXPECT_DEATH(AsanHeapUseAfterFree(), "ABORTING");

  // Register a second error callback which will set should_exit_cleanly.
  AsanService::GetInstance()->AddErrorCallback(
      [](const char* reason, bool* should_exit_cleanly) {
        AsanService::GetInstance()->Log("\nShouldExitCleanly");
        *should_exit_cleanly = true;
      });

  // Check that we now exit instead of crashing.
  EXPECT_EXIT(AsanHeapUseAfterFree(), ExitedCleanly, "ErrorCallback1");
  EXPECT_EXIT(AsanHeapUseAfterFree(), ExitedCleanly, "ShouldExitCleanly");
  EXPECT_EXIT(AsanHeapUseAfterFree(), ExitedCleanly, "EXITING");
}

class AsanTaskTraceTest {
 public:
  AsanTaskTraceTest() = default;

  void Run() {
    task_runner_->PostTask(
        FROM_HERE, BindOnce(&AsanTaskTraceTest::PostingTask, Unretained(this)));
    task_environment_.RunUntilIdle();
  }

 private:
  void PostingTask() {
    task_runner_->PostTask(FROM_HERE, BindOnce(&AsanHeapUseAfterFree));
  }

  test::TaskEnvironment task_environment_;
  const raw_ref<SingleThreadTaskRunner> task_runner_{
      *task_environment_.GetMainThreadTaskRunner()};
};

TEST_F(AsanServiceTest, MAYBE_TaskTraceCallback) {
  AsanTaskTraceTest test;
  // We can't check the symbolization of the task trace, as this will fail on
  // build configurations that don't include symbols. We instead just check
  // that the task trace has the correct number of entries.
  EXPECT_DEATH(test.Run(), "#0 0x.* .*\\n\\s+#1 0x.*");
}

}  // namespace debug
}  // namespace base

#endif  // defined(GTEST_HAS_DEATH_TEST)
#endif  // ADDRESS_SANITIZER
