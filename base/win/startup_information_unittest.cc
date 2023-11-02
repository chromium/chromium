// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/startup_information.h"

#include <windows.h>

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class ScopedProcessTerminator {
 public:
  explicit ScopedProcessTerminator(const PROCESS_INFORMATION& process_info)
      : process_info_(process_info) {}

  ScopedProcessTerminator(const ScopedProcessTerminator&) = delete;
  ScopedProcessTerminator& operator=(const ScopedProcessTerminator&) = delete;

  ~ScopedProcessTerminator() {
    if (process_info_.IsValid())
      ::TerminateProcess(process_info_.process_handle(), 0);
  }

 private:
  base::win::ScopedProcessInformation process_info_;
};

base::win::ScopedHandle CreateInheritedHandle() {
  HANDLE handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(),
                         ::GetCurrentProcess(), &handle,
                         PROCESS_QUERY_LIMITED_INFORMATION, TRUE, 0)) {
    return base::win::ScopedHandle();
  }
  return base::win::ScopedHandle(handle);
}

bool CheckInheritedHandle(HANDLE process, HANDLE check_handle) {
  HANDLE temp_handle;
  if (!::DuplicateHandle(process, check_handle, ::GetCurrentProcess(),
                         &temp_handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return false;
  }
  base::win::ScopedHandle dup_handle(temp_handle);
  return ::GetProcessId(temp_handle) == ::GetCurrentProcessId();
}
}  // namespace

// Verify that only the explicitly specified process handle is inherited.
TEST(StartupInformationTest, InheritStdOut) {
  base::win::ScopedHandle handle_0 = CreateInheritedHandle();
  ASSERT_TRUE(handle_0.is_valid());
  base::win::ScopedHandle handle_1 = CreateInheritedHandle();
  ASSERT_TRUE(handle_1.is_valid());
  ASSERT_NE(handle_0.get(), handle_1.get());

  base::win::StartupInformation startup_info;
  ASSERT_TRUE(startup_info.InitializeProcThreadAttributeList(1));

  HANDLE inherit_process = handle_0.get();
  ASSERT_TRUE(startup_info.UpdateProcThreadAttribute(
      PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherit_process,
      sizeof(inherit_process)));

  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe_path));
  WCHAR cmd_line[] = L"dummy";

  PROCESS_INFORMATION temp_process_info = {};
  ASSERT_TRUE(::CreateProcess(
      exe_path.value().c_str(), cmd_line, nullptr, nullptr, TRUE,
      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr,
      startup_info.startup_info(), &temp_process_info))
      << ::GetLastError();
  ScopedProcessTerminator process(temp_process_info);
  EXPECT_TRUE(CheckInheritedHandle(temp_process_info.hProcess, handle_0.get()));
  EXPECT_FALSE(
      CheckInheritedHandle(temp_process_info.hProcess, handle_1.get()));
}
