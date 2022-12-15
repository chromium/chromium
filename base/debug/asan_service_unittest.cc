// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/asan_service.h"

#if defined(ADDRESS_SANITIZER)

#include <map>
#include <memory>
#include <sstream>

#include "base/debug/asan_invalid_access.h"
#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

class AsanServiceTest : public ::testing::Test {
 protected:
  void SetUp() override { AsanService::GetInstance()->Initialize(); }
};

bool ExitedCleanly(int exit_status) {
  return exit_status == 0;
}

TEST_F(AsanServiceTest, ErrorCallback) {
  // Register an error callback, and check that the output is added.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback1");
  });
  EXPECT_DEATH_IF_SUPPORTED(AsanHeapUseAfterFree(), "ErrorCallback1");

  // Register a second error callback, and check that the output from both
  // callbacks is added.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback2");
  });
  EXPECT_DEATH_IF_SUPPORTED(AsanHeapUseAfterFree(), "ErrorCallback1");
  EXPECT_DEATH_IF_SUPPORTED(AsanHeapUseAfterFree(), "ErrorCallback2");
}

TEST_F(AsanServiceTest, CrashInErrorCallback) {
  // If a nested fault happens, we don't expect to get our custom log messages
  // displayed, but we should still get some part of the ASan report. This
  // matches current ASan recursive fault handling - make sure we don't end up
  // deadlocking.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback1");
    AsanHeapUseAfterFree();
  });
  EXPECT_DEATH_IF_SUPPORTED(AsanHeapUseAfterFree(),
                            "AddressSanitizer: nested bug in the same thread");
}

TEST_F(AsanServiceTest, ShouldExitCleanly) {
  // Register an error callback, and check that the output is added.
  AsanService::GetInstance()->AddErrorCallback([](const char*, bool*) {
    AsanService::GetInstance()->Log("\nErrorCallback1");
  });
  EXPECT_DEATH_IF_SUPPORTED(AsanHeapUseAfterFree(), "ErrorCallback1");
  EXPECT_DEATH_IF_SUPPORTED(AsanHeapUseAfterFree(), "ABORTING");

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

}  // namespace debug
}  // namespace base

#endif  // ADDRESS_SANITIZER