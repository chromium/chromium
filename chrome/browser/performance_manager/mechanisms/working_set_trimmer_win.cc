// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>

#include "base/logging.h"
#include "base/process/process.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {
namespace mechanism {

WorkingSetTrimmerWin::WorkingSetTrimmerWin() = default;
WorkingSetTrimmerWin::~WorkingSetTrimmerWin() = default;

bool WorkingSetTrimmerWin::TrimWorkingSet(const ProcessNode* process_node) {
  // Open a new handle to the process with the specific access needed.
  const base::Process& process = process_node->GetProcess();
  if (!process.IsValid())
    return false;

  base::Process process_copy = base::Process::OpenWithAccess(
      process.Pid(), PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_QUOTA);
  if (!process_copy.IsValid()) {
    DPLOG(ERROR) << "Working set not emptied because process handle could not "
                    "be obtained.";
    return false;
  }

  BOOL empty_working_set_success = ::EmptyWorkingSet(process.Handle());
  DPLOG_IF(ERROR, !empty_working_set_success)
      << "Working set not emptied because EmptyWorkingSet() failed";
  return empty_working_set_success;
}

bool WorkingSetTrimmerWin::PlatformSupportsWorkingSetTrim() {
  return true;
}

}  // namespace mechanism
}  // namespace performance_manager
