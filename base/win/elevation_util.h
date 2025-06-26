// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_ELEVATION_UTIL_H_
#define BASE_WIN_ELEVATION_UTIL_H_

#include <optional>
#include <string>

#include "base/base_export.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
}  // namespace base

namespace base::win {

// Returns the process id for `explorer.exe`.
BASE_EXPORT ProcessId GetExplorerPid();

// Returns `true` if `process_id` is running at medium integrity.
BASE_EXPORT bool IsProcessRunningAtMediumOrLower(ProcessId process_id);

// Runs `command_line` de-elevated and returns the spawned process. Returns a
// Windows error code on failure.
BASE_EXPORT expected<Process, DWORD> RunDeElevated(
    const CommandLine& command_line);

// Runs `command_line` de-elevated. The function does not wait for the spawned
// process.
BASE_EXPORT HRESULT RunDeElevatedNoWait(const CommandLine& command_line);

// Runs `path` de-elevated using `IShellDispatch2::ShellExecute`. `path`
// specifies the file or object on which to execute the default verb (typically
// "open"). If `path` specifies an executable file, `parameters` specifies the
// parameters to be passed to the executable. If `current_directory` is not
// specified, the current directory of the current process is used as the
// default. `start_hidden` will influence the show command. The function does
// not wait for the spawned process. N.B. this function requires COM to be
// initialized.
BASE_EXPORT HRESULT RunDeElevatedNoWait(
    const std::wstring& path,
    const std::wstring& parameters,
    std::optional<std::wstring_view> current_directory = std::nullopt,
    bool start_hidden = false);

}  // namespace base::win

#endif  // BASE_WIN_ELEVATION_UTIL_H_
