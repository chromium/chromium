// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/utils.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
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

  // First try to fast-kill the process, if it's just running a single tab.
  bool succeed = render_process_host->FastShutdownIfPossible(1u, false);
  AttemptFastKillForDiscardResult result =
      succeed ? AttemptFastKillForDiscardResult::kKilled
              : AttemptFastKillForDiscardResult::kSkipped;

#if BUILDFLAG(IS_CHROMEOS)
  if (!succeed &&
      discard_reason == ::mojom::LifecycleUnitDiscardReason::URGENT) {
    // We avoid fast shutdown on tabs with beforeunload handlers on the main
    // frame, as that is often an indication of unsaved user state.
    static const bool should_ignore_workers =
        features::kUrgentDiscardIgnoreWorkers.Get();
    if (!main_frame->GetSuddenTerminationDisablerState(
            blink::mojom::SuddenTerminationDisablerType::
                kBeforeUnloadHandler) &&
        render_process_host->FastShutdownIfPossible(
            1u, /*skip_unload_handlers=*/true,
            /*ignore_workers=*/should_ignore_workers)) {
      result =
          should_ignore_workers
              ? AttemptFastKillForDiscardResult::
                    kKilledWithoutUnloadHandlersAndWorkers
              : AttemptFastKillForDiscardResult::kKilledWithoutUnloadHandlers;
    }
  }
#endif
  base::UmaHistogramEnumeration("Discarding.AttemptFastKillForDiscardResult",
                                result);
}

}  // namespace resource_coordinator
