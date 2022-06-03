// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <winternl.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace base {
namespace win {

namespace testing {
extern "C" bool __declspec(dllexport) RunTest();
}  // namespace testing

TEST(ScopedHandleTest, ScopedHandle) {
  // Any illegal error code will do. We just need to test that it is preserved
  // by ScopedHandle to avoid bug 528394.
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

TEST(ScopedHandleTest, HandleVerifierTrackedHasBeenClosed) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);
  using NtCloseFunc = decltype(&::NtClose);
  NtCloseFunc ntclose = reinterpret_cast<NtCloseFunc>(
      GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtClose"));
  ASSERT_NE(nullptr, ntclose);

  ASSERT_DEATH(
      {
        base::win::ScopedHandle handle_holder(handle);
        ntclose(handle);
        // Destructing a ScopedHandle with an illegally closed handle should
        // fail.
      },
      "");
}

TEST(ScopedHandleTest, HandleVerifierDoubleTracking) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  base::win::CheckedScopedHandle handle_holder(handle);

  ASSERT_DEATH({ base::win::CheckedScopedHandle handle_holder2(handle); }, "");
}

TEST(ScopedHandleTest, HandleVerifierWrongOwner) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  base::win::CheckedScopedHandle handle_holder(handle);
  ASSERT_DEATH(
      {
        base::win::CheckedScopedHandle handle_holder2;
        handle_holder2.handle_ = handle;
      },
      "");
  ASSERT_TRUE(handle_holder.IsValid());
  handle_holder.Close();
}

TEST(ScopedHandleTest, HandleVerifierUntrackedHandle) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  ASSERT_DEATH(
      {
        base::win::CheckedScopedHandle handle_holder;
        handle_holder.handle_ = handle;
      },
      "");

  ASSERT_TRUE(::CloseHandle(handle));
}

// Under ASan, the multi-process test crashes during process shutdown for
// unknown reasons. Disable it for now. http://crbug.com/685262
#if defined(ADDRESS_SANITIZER)
#define MAYBE_MultiProcess DISABLED_MultiProcess
#else
#define MAYBE_MultiProcess MultiProcess
#endif

TEST(ScopedHandleTest, MAYBE_MultiProcess) {
  // Initializing ICU in the child process causes a scoped handle to be created
  // before the test gets a chance to test the race condition, so disable ICU
  // for the child process here.
  CommandLine command_line(base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitch(switches::kTestDoNotInitializeIcu);

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
