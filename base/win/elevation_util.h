// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_ELEVATION_UTIL_H_
#define BASE_WIN_ELEVATION_UTIL_H_

#include "base/base_export.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
}  // namespace base

namespace base::win {

// Returns the process id for `explorer.exe`.
BASE_EXPORT ProcessId GetExplorerPid();

// Returns `true` if `process_id` is running at medium integrity.
BASE_EXPORT bool IsProcessRunningAtMediumOrLower(ProcessId process_id);

// Runs `command_line` de-elevated and returns the spawned process. Returns an
// invalid process on failure. `::GetLastError` can be used to get the last
// error in the failure case.
BASE_EXPORT Process RunDeElevated(const CommandLine& command_line);

}  // namespace base::win

#endif  // BASE_WIN_ELEVATION_UTIL_H_
