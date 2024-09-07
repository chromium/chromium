// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {
namespace mechanism {
namespace {

// The chromeos kernel supports per-process reclaim if there exists a /reclaim
// file in a procfs node. We will simply stat /proc/self/reclaim to detect this
// support.
bool KernelSupportsReclaim() {
  return base::PathExists(base::FilePath("/proc/self/reclaim"));
}

void ReclaimWorkingSet(base::ProcessId pid) {
  const std::string reclaim_file = base::StringPrintf("/proc/%d/reclaim", pid);
  const std::string kReclaimMode = "all";
  bool success = base::WriteFile(base::FilePath(reclaim_file), kReclaimMode);

  // We won't log an error if reclaim failed due to the process being dead.
  PLOG_IF(ERROR, !success && errno != ENOENT)
      << "Write failed on " << reclaim_file << " mode: " << kReclaimMode;
}

content::BrowserContext* GetContext() {
  // For production, always use the primary user profile. ARCVM does not
  // support non-primary profiles. |g_browser_process| can be nullptr during
  // browser shutdown.
  if (g_browser_process && g_browser_process->profile_manager())
    return g_browser_process->profile_manager()->GetPrimaryUserProfile();
  return nullptr;
}

}  // namespace

// static
std::unique_ptr<WorkingSetTrimmerChromeOS>
WorkingSetTrimmerChromeOS::CreateForTesting(content::BrowserContext* context) {
  auto* policy = new WorkingSetTrimmerChromeOS();
  policy->context_for_testing_ = context;
  return base::WrapUnique(policy);
}

WorkingSetTrimmerChromeOS::WorkingSetTrimmerChromeOS() = default;
WorkingSetTrimmerChromeOS::~WorkingSetTrimmerChromeOS() = default;

bool WorkingSetTrimmerChromeOS::PlatformSupportsWorkingSetTrim() {
  static const bool kPlatformSupported = KernelSupportsReclaim();
  return kPlatformSupported;
}

void WorkingSetTrimmerChromeOS::TrimWorkingSet(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(features::kRunOnMainThreadSync)) {
    // Main thread is not allowed to block.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::BindOnce(&ReclaimWorkingSet, pid));
  } else {
    ReclaimWorkingSet(pid);
  }
}

void WorkingSetTrimmerChromeOS::TrimArcVmWorkingSet(
    TrimArcVmWorkingSetCallback callback,
    ArcVmReclaimType reclaim_type,
    int page_limit) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  arc::ArcVmWorkingSetTrimExecutor::Trim(
      context_for_testing_ ? context_for_testing_.get() : GetContext(),
      std::move(callback), reclaim_type, page_limit);
}

void WorkingSetTrimmerChromeOS::TrimWorkingSet(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_node->GetProcess().IsValid()) {
    TrimWorkingSet(process_node->GetProcessId());
  }
}

}  // namespace mechanism
}  // namespace performance_manager
