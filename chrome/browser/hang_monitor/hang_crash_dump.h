// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HANG_MONITOR_HANG_CRASH_DUMP_H_
#define CHROME_BROWSER_HANG_MONITOR_HANG_CRASH_DUMP_H_

#include "base/process/process_handle.h"

// Attempts to generate a crash dump for the child process represented by
// |handle|. This is not implemented on all platofrms.
void CrashDumpHungChildProcess(base::ProcessHandle handle);

#endif  // CHROME_BROWSER_HANG_MONITOR_HANG_CRASH_DUMP_H_
