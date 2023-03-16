// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
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
  const std::string reclaim_file = base::StringPrintf("/proc/%d/reclaim", pid);
  const std::string kReclaimMode = "all";
  [[maybe_unused]] bool success =
      base::WriteFile(base::FilePath(reclaim_file), kReclaimMode);

  // We won't log an error if reclaim failed due to the process being dead.
  PLOG_IF(ERROR, success && errno != ENOENT)
      << "Write failed on " << reclaim_file << " mode: " << kReclaimMode;
}

void WorkingSetTrimmerChromeOS::TrimArcVmWorkingSet(
    TrimArcVmWorkingSetCallback callback,
    ArcVmReclaimType reclaim_type,
    int page_limit) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(ArcVmReclaimType::kReclaimNone, reclaim_type);
  const char* error = nullptr;

  // Before trimming, drop ARCVM's page caches.
  content::BrowserContext* context =
      context_for_testing_ ? context_for_testing_ : GetContext();
  if (!context)
    error = "BrowserContext unavailable";

  auto* bridge =
      context ? arc::ArcMemoryBridge::GetForBrowserContext(context) : nullptr;
  if (!bridge)
    error = "ArcMemoryBridge unavailable";

  if (error) {
    LOG(ERROR) << error;
    if (reclaim_type == ArcVmReclaimType::kReclaimGuestPageCaches) {
      // Failed to drop caches. When the type if kReclaimGuestPageCaches, run
      // the |callback| now with the |error| message. No further action is
      // necessary.
      std::move(callback).Run(false, error);
    } else {
      // Otherwise, continue without dropping them.
      OnDropArcVmCaches(std::move(callback), reclaim_type, page_limit,
                        /*result=*/false);
    }
    return;
  }

  bridge->DropCaches(base::BindOnce(
      &WorkingSetTrimmerChromeOS::OnDropArcVmCaches, weak_factory_.GetWeakPtr(),
      std::move(callback), reclaim_type, page_limit));
}

void WorkingSetTrimmerChromeOS::OnDropArcVmCaches(
    TrimArcVmWorkingSetCallback callback,
    ArcVmReclaimType reclaim_type,
    int page_limit,
    bool result) {
  constexpr const char kErrorMessage[] =
      "Failed to drop ARCVM's guest page caches";
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LOG_IF(WARNING, !result) << kErrorMessage;

  if (reclaim_type == ArcVmReclaimType::kReclaimGuestPageCaches) {
    // TrimVmMemory() is unnecessary. Just run the |callback| with the
    // DropCaches() result.
    std::move(callback).Run(result, result ? "" : kErrorMessage);
    return;
  }

  // Do the actual VM trimming regardless of the |result|. When "ArcGuestZram"
  // feature is enabled, guest memory is locked and should be reclaimed from
  // guest through ArcMemoryBridge's reclaim API (if "guest_reclaim_enabled"
  // param is enabled). Otherwise the memory should be reclaimed from host
  // through ArcSessionManager's TrimVmMemory.
  if (base::FeatureList::IsEnabled(arc::kGuestZram) &&
      arc::kGuestReclaimEnabled.Get()) {
    content::BrowserContext* context =
        context_for_testing_ ? context_for_testing_ : GetContext();
    if (!context) {
      LogErrorAndInvokeCallback("BrowserContext unavailable",
                                std::move(callback));
      return;
    }

    auto* bridge =
        context ? arc::ArcMemoryBridge::GetForBrowserContext(context) : nullptr;
    if (!bridge) {
      LogErrorAndInvokeCallback("ArcMemoryBridge unavailable",
                                std::move(callback));
      return;
    }

    auto reclaim_request = arc::mojom::ReclaimRequest::New(
        arc::kGuestReclaimOnlyAnonymous.Get() ? arc::mojom::ReclaimType::ANON
                                              : arc::mojom::ReclaimType::ALL);
    bridge->Reclaim(
        std::move(reclaim_request),
        base::BindOnce(&WorkingSetTrimmerChromeOS::OnArcVmMemoryGuestReclaim,
                       weak_factory_.GetMutableWeakPtr(), std::move(callback)));
  } else {
    arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
    if (!arc_session_manager) {
      LogErrorAndInvokeCallback("ArcSessionManager unavailable",
                                std::move(callback));
      return;
    }

    arc_session_manager->TrimVmMemory(std::move(callback), page_limit);
  }
}

void WorkingSetTrimmerChromeOS::OnArcVmMemoryGuestReclaim(
    TrimArcVmWorkingSetCallback callback,
    arc::mojom::ReclaimResultPtr result) {
  VLOG(2) << "Finished trimming memory from guest. " << result->reclaimed
          << " processes were reclaimed successfully. " << result->unreclaimed
          << " processes were not reclaimed.";
  if (result->reclaimed == 0) {
    std::move(callback).Run(false, "No guest process was reclaimed");
  } else {
    std::move(callback).Run(true, "");
  }
}

void WorkingSetTrimmerChromeOS::LogErrorAndInvokeCallback(
    const char* error,
    TrimArcVmWorkingSetCallback callback) {
  LOG(ERROR) << error;
  std::move(callback).Run(false, error);
}

void WorkingSetTrimmerChromeOS::TrimWorkingSet(
    const ProcessNode* process_node) {
  if (process_node->GetProcess().IsValid()) {
    TrimWorkingSet(process_node->GetProcessId());
  }
}

}  // namespace mechanism
}  // namespace performance_manager
