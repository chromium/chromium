// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/memory/arc_memory_bridge.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {
namespace mechanism {
namespace {

constexpr char kBrowserContextErrorMessage[] = "BrowserContext unavailable";

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
  // Main thread is not allowed to block.
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(&ReclaimWorkingSet, pid));
}

void WorkingSetTrimmerChromeOS::TrimArcVmWorkingSet(
    TrimArcVmWorkingSetCallback callback,
    ArcVmReclaimType reclaim_type,
    int page_limit) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);
  CHECK_NE(ArcVmReclaimType::kReclaimNone, reclaim_type,
           base::NotFatalUntil::M160);
  const char* error = nullptr;

  if (is_trimming_arcvm_) {
    std::move(callback).Run(false,
                            "ArcVm is trimming, skip this trim request.");
    return;
  }
  is_trimming_arcvm_ = true;
  callback = std::move(callback).Then(base::BindOnce(
      [](base::WeakPtr<WorkingSetTrimmerChromeOS> self) {
        if (self) {
          self->is_trimming_arcvm_ = false;
        }
      },
      weak_factory_.GetWeakPtr()));

  // Before trimming, drop ARCVM's page caches.
  content::BrowserContext* context =
      context_for_testing_ ? context_for_testing_.get() : GetContext();
  if (!context) {
    error = kBrowserContextErrorMessage;
  }

  auto* bridge =
      context ? arc::ArcMemoryBridge::GetForBrowserContext(context) : nullptr;
  if (!bridge) {
    error = "ArcMemoryBridge unavailable";
  }

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

  if (base::FeatureList::IsEnabled(arc::kSkipDropCaches)) {
    // If the feature to skip dropping caches is enabled, continue without
    // dropping them.
    VLOG(1) << "ARC skip drop caches feature enabled. Proceeding without "
               "forced cache drop.";
    OnDropArcVmCaches(std::move(callback), reclaim_type, page_limit,
                      /*result=*/true);
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
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  LOG_IF(WARNING, !result) << kErrorMessage;

  if (reclaim_type == ArcVmReclaimType::kReclaimGuestPageCaches) {
    // TrimVmMemory() is unnecessary. Just run the |callback| with the
    // DropCaches() result.
    std::move(callback).Run(result, result ? "" : kErrorMessage);
    return;
  }

  // kReclaimNone is disallowed in TrimArcVmWorkingSet() and
  // kReclaimGuestPageCaches returned above, so only kReclaimAll remains.
  CHECK_EQ(reclaim_type, ArcVmReclaimType::kReclaimAll);

  // Do the actual VM trimming regardless of the |result|. When ARCVM swap
  // is enabled and not locked, try to reclaim from the guest first to avoid
  // swap shuffle, where a page owned by the guest is swapped out from the host
  // and then swapped in to be swapped out on the guest. Otherwise the memory
  // should be reclaimed from host only, through ArcSessionManager's
  // TrimVmMemory if requested.
  if (base::FeatureList::IsEnabled(arc::kGuestSwap) &&
      arc::kGuestReclaimEnabled.Get()) {
    content::BrowserContext* context =
        context_for_testing_ ? context_for_testing_.get() : GetContext();
    if (!context) {
      LogErrorAndInvokeCallback(kBrowserContextErrorMessage,
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

    const bool should_reclaim_from_host = arc::kVirtualSwapEnabled.Get();

    bridge->Reclaim(
        std::move(reclaim_request),
        base::BindOnce(
            &WorkingSetTrimmerChromeOS::OnArcVmMemoryGuestReclaim,
            weak_factory_.GetWeakPtr(), std::make_unique<base::ElapsedTimer>(),
            std::move(callback), should_reclaim_from_host, page_limit));
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
    std::unique_ptr<base::ElapsedTimer> elapsed_timer,
    TrimArcVmWorkingSetCallback callback,
    bool should_reclaim_from_host,
    int host_reclaim_page_limit,
    arc::mojom::ReclaimResultPtr result) {
  VLOG(2) << "Finished trimming memory from guest. " << result->reclaimed
          << " processes were reclaimed successfully. " << result->unreclaimed
          << " processes were not reclaimed.";
  base::UmaHistogramBoolean("Arc.GuestZram.SuccessfulReclaim",
                            (result->reclaimed > 0));

  constexpr const char kGuestReclaimErrorMessage[] =
      "No guest process was reclaimed";
  bool guest_reclaim_succedded = result->reclaimed > 0;
  if (!guest_reclaim_succedded) {
    LOG(WARNING) << kGuestReclaimErrorMessage;
  } else {
    base::UmaHistogramCounts1000("Arc.GuestZram.ReclaimedProcess",
                                 result->reclaimed);
    base::UmaHistogramCounts1000("Arc.GuestZram.UnreclaimedProcess",
                                 result->unreclaimed);
    base::UmaHistogramMediumTimes("Arc.GuestZram.TotalReclaimTime",
                                  elapsed_timer->Elapsed());
  }

  if (!should_reclaim_from_host) {
    std::move(callback).Run(
        guest_reclaim_succedded,
        guest_reclaim_succedded ? "" : kGuestReclaimErrorMessage);
    return;
  }

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager) {
    LogErrorAndInvokeCallback("ArcSessionManager unavailable",
                              std::move(callback));
    return;
  }
  arc_session_manager->TrimVmMemory(std::move(callback),
                                    host_reclaim_page_limit);
}

void WorkingSetTrimmerChromeOS::LogErrorAndInvokeCallback(
    const char* error,
    TrimArcVmWorkingSetCallback callback) {
  LOG(ERROR) << error;
  std::move(callback).Run(false, error);
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
