// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/elevation_util.h"

#include <windows.h>

#include <shlobj.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_info.h"
#include "base/win/access_token.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace base::win {

ProcessId GetExplorerPid() {
  const HWND hwnd = ::GetShellWindow();
  ProcessId pid = 0;
  return hwnd && ::GetWindowThreadProcessId(hwnd, &pid) ? pid : kNullProcessId;
}

bool IsProcessRunningAtMediumOrLower(ProcessId process_id) {
  IntegrityLevel level = GetProcessIntegrityLevel(process_id);
  return level != INTEGRITY_UNKNOWN && level <= MEDIUM_INTEGRITY;
}

// Based on
// https://learn.microsoft.com/en-us/archive/blogs/aaron_margosis/faq-how-do-i-start-a-program-as-the-desktop-user-from-an-elevated-app.
Process RunDeElevated(const CommandLine& command_line) {
  if (!::IsUserAnAdmin()) {
    return LaunchProcess(command_line, {});
  }

  ProcessId explorer_pid = GetExplorerPid();
  if (!explorer_pid || !IsProcessRunningAtMediumOrLower(explorer_pid)) {
    return Process();
  }

  auto shell_process =
      Process::OpenWithAccess(explorer_pid, PROCESS_QUERY_LIMITED_INFORMATION);
  if (!shell_process.IsValid()) {
    return Process();
  }

  auto token = AccessToken::FromProcess(
      ::GetCurrentProcess(), /*impersonation=*/false, MAXIMUM_ALLOWED);
  if (!token) {
    return Process();
  }
  auto previous_impersonate = token->SetPrivilege(SE_IMPERSONATE_NAME, true);
  if (!previous_impersonate) {
    return Process();
  }
  absl::Cleanup restore_previous_privileges = [&] {
    token->SetPrivilege(SE_IMPERSONATE_NAME, *previous_impersonate);
  };

  auto shell_token = AccessToken::FromProcess(
      shell_process.Handle(), /*impersonation=*/false, TOKEN_DUPLICATE);
  if (!shell_token) {
    return Process();
  }

  auto duplicated_shell_token = shell_token->DuplicatePrimary(
      TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE |
      TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID);
  if (!duplicated_shell_token) {
    return Process();
  }

  StartupInformation startupinfo;
  PROCESS_INFORMATION pi = {};
  if (!::CreateProcessWithTokenW(duplicated_shell_token->get(), 0,
                                 command_line.GetProgram().value().c_str(),
                                 command_line.GetCommandLineString().data(), 0,
                                 nullptr, nullptr, startupinfo.startup_info(),
                                 &pi)) {
    return Process();
  }
  ScopedProcessInformation process_info(pi);
  Process process(process_info.TakeProcessHandle());
  const DWORD pid = process.Pid();
  VLOG(1) << __func__ << ": Started process, PID: " << pid;

  // Allow the spawned process to show windows in the foreground.
  if (!::AllowSetForegroundWindow(pid)) {
    VPLOG(1) << __func__ << ": ::AllowSetForegroundWindow failed";
  }

  return process;
}

}  // namespace base::win
