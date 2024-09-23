// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"

#include <windows.h>

#include <winternl.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace base {
namespace win {

namespace {

std::string FailureMessage(const std::string& msg) {
#if defined(NDEBUG) && defined(OFFICIAL_BUILD)
  // Official release builds strip all fatal messages for saving binary size,
  // see base/check.h.
  return "";
#else
  return msg;
#endif  // defined(NDEBUG) && defined(OFFICIAL_BUILD)
}

}  // namespace

namespace testing {
extern "C" bool __declspec(dllexport) RunTest();
}  // namespace testing

using ScopedHandleTest = ::testing::Test;
using ScopedHandleDeathTest = ::testing::Test;

TEST_F(ScopedHandleTest, ScopedHandle) {
  // Any illegal error code will do. We just need to test that it is preserved
  // by ScopedHandle to avoid https://crbug.com/528394.
  const DWORD magic_error = 0x12345678;

  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  // Call SetLastError after creating the handle.
  ::SetLastError(magic_error);
  base::win::ScopedHandle handle_holder(handle);
  EXPECT_EQ(magic_error, ::GetLastError());

  // Create a new handle and then set LastError again.
  handle = ::CreateMutex(nullptr, false, nullptr);
  ::SetLastError(magic_error);
  handle_holder.Set(handle);
  EXPECT_EQ(magic_error, ::GetLastError());

  // Create a new handle and then set LastError again.
  handle = ::CreateMutex(nullptr, false, nullptr);
  base::win::ScopedHandle handle_source(handle);
  ::SetLastError(magic_error);
  handle_holder = std::move(handle_source);
  EXPECT_EQ(magic_error, ::GetLastError());
}

TEST_F(ScopedHandleDeathTest, HandleVerifierTrackedHasBeenClosed) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  ASSERT_DEATH(
      {
        base::win::ScopedHandle handle_holder(handle);
        ::NtClose(handle);
        // Destructing a ScopedHandle with an illegally closed handle should
        // fail.
      },
      FailureMessage("CloseHandle failed"));
}

TEST_F(ScopedHandleDeathTest, HandleVerifierCloseTrackedHandle) {
  ASSERT_DEATH(
      {
        HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
        ASSERT_NE(HANDLE(nullptr), handle);

        // Start tracking the handle so that closes outside of the checker are
        // caught.
        base::win::CheckedScopedHandle handle_holder(handle);

        // Closing a tracked handle using ::CloseHandle should crash due to hook
        // noticing the illegal close.
        ::CloseHandle(handle);
      },
      // This test must match the CloseHandleHook causing this failure, because
      // if the hook doesn't crash and instead the handle is double closed by
      // the `handle_holder` going out of scope, then there is still a crash,
      // but a different crash and one we are not explicitly testing here. This
      // other crash is tested in HandleVerifierTrackedHasBeenClosed above.
      FailureMessage("CloseHandleHook validation failure"));
}

TEST_F(ScopedHandleDeathTest, HandleVerifierDoubleTracking) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  base::win::CheckedScopedHandle handle_holder(handle);

  ASSERT_DEATH({ base::win::CheckedScopedHandle handle_holder2(handle); },
               FailureMessage("Handle Already Tracked"));
}

TEST_F(ScopedHandleDeathTest, HandleVerifierWrongOwner) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  base::win::CheckedScopedHandle handle_holder(handle);
  ASSERT_DEATH(
      {
        base::win::CheckedScopedHandle handle_holder2;
        handle_holder2.handle_ = handle;
      },
      FailureMessage("Closing a handle owned by something else"));
  ASSERT_TRUE(handle_holder.is_valid());
  handle_holder.Close();
}

TEST_F(ScopedHandleDeathTest, HandleVerifierUntrackedHandle) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  ASSERT_DEATH(
      {
        base::win::CheckedScopedHandle handle_holder;
        handle_holder.handle_ = handle;
      },
      FailureMessage("Closing an untracked handle"));

  ASSERT_TRUE(::CloseHandle(handle));
}

// Under ASan, the multi-process test crashes during process shutdown for
// unknown reasons. Disable it for now. http://crbug.com/685262
#if defined(ADDRESS_SANITIZER)
#define MAYBE_MultiProcess DISABLED_MultiProcess
#else
#define MAYBE_MultiProcess MultiProcess
#endif

TEST_F(ScopedHandleTest, MAYBE_MultiProcess) {
  CommandLine command_line(base::GetMultiProcessTestChildBaseCommandLine());

  base::Process test_child_process = base::SpawnMultiProcessTestChild(
      "HandleVerifierChildProcess", command_line, LaunchOptions());

  int rv = -1;
  ASSERT_TRUE(test_child_process.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(HandleVerifierChildProcess) {
  ScopedNativeLibrary module(
      FilePath(FILE_PATH_LITERAL("scoped_handle_test_dll.dll")));

  if (!module.is_valid())
    return 1;
  auto run_test_function = reinterpret_cast<decltype(&testing::RunTest)>(
      module.GetFunctionPointer("RunTest"));
  if (!run_test_function)
    return 1;
  if (!run_test_function())
    return 1;

  return 0;
}

}  // namespace win
}  // namespace base
