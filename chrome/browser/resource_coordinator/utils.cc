// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/utils.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom.h"

namespace resource_coordinator {

TabLifecycleUnitSource* GetTabLifecycleUnitSource() {
  DCHECK(g_browser_process);
  auto* source = g_browser_process->resource_coordinator_parts()
                     ->tab_lifecycle_unit_source();
  DCHECK(source);
  return source;
}

void AttemptFastKillForDiscard(
    content::WebContents* web_contents,
    ::mojom::LifecycleUnitDiscardReason discard_reason) {
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  CHECK(main_frame);
  content::RenderProcessHost* render_process_host = main_frame->GetProcess();
  CHECK(render_process_host);

  const bool web_contents_discard_enabled =
      base::FeatureList::IsEnabled(features::kWebContentsDiscard);

  bool allow_skip_unload = false;
  if (discard_reason == ::mojom::LifecycleUnitDiscardReason::URGENT) {
#if BUILDFLAG(IS_CHROMEOS)
    allow_skip_unload = true;
#else
    allow_skip_unload = web_contents_discard_enabled;
#endif
  }

  const bool should_ignore_workers =
#if BUILDFLAG(IS_CHROMEOS)
      discard_reason == ::mojom::LifecycleUnitDiscardReason::URGENT &&
      features::kUrgentDiscardIgnoreWorkers.Get();
#else
      false;
#endif

  bool has_before_unload = false;
  absl::flat_hash_set<content::RenderProcessHost*> subframe_processes;
  main_frame->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (rfh->GetSuddenTerminationDisablerState(
            blink::mojom::SuddenTerminationDisablerType::
                kBeforeUnloadHandler)) {
      has_before_unload = true;
    }
    content::RenderProcessHost* process = rfh->GetProcess();
    if (process && process != render_process_host) {
      subframe_processes.insert(process);
    }
  });

  if (web_contents_discard_enabled) {
    // Attempt fast shutdown for any dedicated out-of-process iframe (OOPIF)
    // processes before shutting down the main frame process. Shutting down the
    // main frame process triggers RenderProcessGone() -> ResetChildren(), which
    // would detach child frames and prevent
    // GetOutermostMainFrameCountForFastShutdown from associating the OOPIF
    // process with this tab's active main frame.
    for (content::RenderProcessHost* process : subframe_processes) {
      bool subframe_killed = process->FastShutdownIfPossible(
          1u, /*skip_unload_handlers=*/false,
          /*ignore_workers=*/false,
          /*ignore_keep_alive=*/false,
          /*ignore_pending_reuse=*/false,
          /*use_outermost_main_frame_check=*/true);
      if (!subframe_killed && allow_skip_unload && !has_before_unload) {
        process->FastShutdownIfPossible(
            1u, /*skip_unload_handlers=*/true,
            /*ignore_workers=*/should_ignore_workers,
            /*ignore_keep_alive=*/false,
            /*ignore_pending_reuse=*/false,
            /*use_outermost_main_frame_check=*/true);
      }
    }
  }

  // Now try to fast-kill the main frame process, if it's just running a single
  // tab.
  bool succeed = render_process_host->FastShutdownIfPossible(
      1u,
      /*skip_unload_handlers=*/false,
      /*ignore_workers=*/false,
      /*ignore_keep_alive=*/false,
      /*ignore_pending_reuse=*/false,
      /*use_outermost_main_frame_check=*/web_contents_discard_enabled);
  AttemptFastKillForDiscardResult result =
      succeed ? AttemptFastKillForDiscardResult::kKilled
              : AttemptFastKillForDiscardResult::kSkipped;

  if (!succeed && allow_skip_unload && !has_before_unload) {
    // We avoid fast shutdown on tabs with beforeunload handlers, as that is
    // often an indication of unsaved user state.
    if (render_process_host->FastShutdownIfPossible(
            1u, /*skip_unload_handlers=*/true,
            /*ignore_workers=*/should_ignore_workers,
            /*ignore_keep_alive=*/false,
            /*ignore_pending_reuse=*/false,
            /*use_outermost_main_frame_check=*/web_contents_discard_enabled)) {
      result =
          should_ignore_workers
              ? AttemptFastKillForDiscardResult::
                    kKilledWithoutUnloadHandlersAndWorkers
              : AttemptFastKillForDiscardResult::kKilledWithoutUnloadHandlers;
    }
  }

  base::UmaHistogramEnumeration("Discarding.AttemptFastKillForDiscardResult",
                                result);
}

content::WebContents* DiscardLeastImportantTab(
    ::mojom::LifecycleUnitDiscardReason discard_reason,
    bool ignore_recent_visibility,
    std::optional<absl::flat_hash_set<base::UnguessableToken>>
        allowed_browser_context_ids) {
  performance_manager::Graph* graph =
      performance_manager::PerformanceManager::GetGraph();
  CHECK(graph);

  auto* discarding_helper =
      performance_manager::policies::PageDiscardingHelper::GetFromGraph(graph);
  if (!discarding_helper) {
    return nullptr;
  }

  return discarding_helper
      ->DiscardAPage(discard_reason, ignore_recent_visibility,
                     std::move(allowed_browser_context_ids))
      .first_content_after_discard;
}

}  // namespace resource_coordinator
