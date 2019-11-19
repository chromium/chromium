// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {
namespace mechanism {
namespace {

// The chromeos kernel supports per-process reclaim if there exists a /reclaim
// file in a procfs node. We will simply stat /proc/self/reclaim to detect this
// support.
bool KernelSupportsReclaim() {
  return base::PathExists(base::FilePath("/proc/self/reclaim"));
}

}  // namespace

WorkingSetTrimmerChromeOS::WorkingSetTrimmerChromeOS() = default;
WorkingSetTrimmerChromeOS::~WorkingSetTrimmerChromeOS() = default;

bool WorkingSetTrimmerChromeOS::PlatformSupportsWorkingSetTrim() {
  static const bool kPlatformSupported = KernelSupportsReclaim();
  return kPlatformSupported;
}

bool WorkingSetTrimmerChromeOS::TrimWorkingSet(
    const ProcessNode* process_node) {
  if (!process_node->GetProcess().IsValid())
    return false;

  const std::string reclaim_file =
      base::StringPrintf("/proc/%d/reclaim", process_node->GetProcessId());
  const std::string kReclaimMode = "all";
  ssize_t written = base::WriteFile(base::FilePath(reclaim_file),
                                    kReclaimMode.c_str(), kReclaimMode.size());

  // We won't log an error if reclaim failed due to the process being dead.
  PLOG_IF(ERROR, written < 0 && errno != ENOENT)
      << "Write failed on " << reclaim_file << " mode: " << kReclaimMode;
  return written > 0;
}

}  // namespace mechanism
}  // namespace performance_manager
