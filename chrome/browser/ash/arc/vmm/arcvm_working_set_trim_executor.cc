// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {
constexpr char BROWSER_CONTEXT_ERROR_MSG[] = "BrowserContext unavailable";
}

bool ArcVmWorkingSetTrimExecutor::is_trimming_ = false;

void ArcVmWorkingSetTrimExecutor::Trim(content::BrowserContext* context,
                                       ResultCallback callback,
                                       ArcVmReclaimType reclaim_type,
                                       int page_limit) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(ArcVmReclaimType::kReclaimNone, reclaim_type);
  const char* error = nullptr;

  if (ArcVmWorkingSetTrimExecutor::is_trimming_) {
    std::move(callback).Run(false,
                            "ArcVm is trimming, skip this trim request.");
    return;
  }
  ArcVmWorkingSetTrimExecutor::is_trimming_ = true;
  // Reset `is_trimming` after called the result callback.
  callback = std::move(callback).Then(
      base::BindOnce([](bool& is_trimming_state) { is_trimming_state = false; },
                     std::ref(ArcVmWorkingSetTrimExecutor::is_trimming_)));

  // Before trimming, drop ARCVM's page caches.
  if (!context) {
    error = BROWSER_CONTEXT_ERROR_MSG;
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
      OnDropArcVmCaches(context, std::move(callback), reclaim_type, page_limit,
                        /*result=*/false);
    }
    return;
  }

  if (base::FeatureList::IsEnabled(arc::kSkipDropCaches)) {
    // If the feature to skip dropping caches is enabled, continue without
    // dropping them.
    VLOG(1) << "ARC skip drop caches feature enabled. Proceeding without "
               "forced cache drop.";
    OnDropArcVmCaches(context, std::move(callback), reclaim_type, page_limit,
                      /*result=*/true);
    return;
  }

  bridge->DropCaches(
      base::BindOnce(&ArcVmWorkingSetTrimExecutor::OnDropArcVmCaches, context,
                     std::move(callback), reclaim_type, page_limit));
}

void ArcVmWorkingSetTrimExecutor::OnDropArcVmCaches(
    content::BrowserContext* context,
    ResultCallback callback,
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

  // Do the actual VM trimming regardless of the |result|. When ARCVM swap
  // is enabled and not locked, try to reclaim from the guest first to avoid
  // swap shuffle, where a page owned by the guest is swapped out from the host
  // and then swapped in to be swapped out on the guest. Otherwise the memory
  // should be reclaimed from host only, through ArcSessionManager's
  // TrimVmMemory if requested.
  if (base::FeatureList::IsEnabled(arc::kGuestSwap) &&
      arc::kGuestReclaimEnabled.Get()) {
    if (!context) {
      LogErrorAndInvokeCallback(BROWSER_CONTEXT_ERROR_MSG, std::move(callback));
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

    const bool should_reclaim_from_host =
        reclaim_type == ArcVmReclaimType::kReclaimAll &&
        arc::kVirtualSwapEnabled.Get() &&
        !base::FeatureList::IsEnabled(arc::kLockGuestMemory);

    bridge->Reclaim(
        std::move(reclaim_request),
        base::BindOnce(&ArcVmWorkingSetTrimExecutor::OnArcVmMemoryGuestReclaim,
                       std::make_unique<base::ElapsedTimer>(),
                       std::move(callback), should_reclaim_from_host,
                       page_limit));
  } else if (reclaim_type == ArcVmReclaimType::kReclaimAll) {
    arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
    if (!arc_session_manager) {
      LogErrorAndInvokeCallback("ArcSessionManager unavailable",
                                std::move(callback));
      return;
    }
    arc_session_manager->TrimVmMemory(std::move(callback), page_limit);
  } else {
    std::move(callback).Run(true, "");
  }
}

void ArcVmWorkingSetTrimExecutor::OnArcVmMemoryGuestReclaim(
    std::unique_ptr<base::ElapsedTimer> elapsed_timer,
    ResultCallback callback,
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

void ArcVmWorkingSetTrimExecutor::LogErrorAndInvokeCallback(
    const char* error,
    ResultCallback callback) {
  LOG(ERROR) << error;
  std::move(callback).Run(false, error);
}

}  // namespace arc
