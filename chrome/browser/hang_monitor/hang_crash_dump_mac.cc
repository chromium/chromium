// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump.h"

#include "base/process/process.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/common/result_codes.h"

void CrashDumpHungChildProcess(base::ProcessHandle handle) {
  base::PortProvider* provider =
      content::BrowserChildProcessHost::GetPortProvider();
  mach_port_t task_port = provider->TaskForHandle(handle);
  if (task_port != MACH_PORT_NULL) {
    crash_reporter::DumpProcessWithoutCrashing(task_port);
  }
}
