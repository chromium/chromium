// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/session_restore_page_load_metrics_observer.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/page_transition_types.h"

namespace internal {

const char kHistogramSessionRestoreForegroundTabFirstPaint[] =
    "TabManager.Experimental.SessionRestore.ForegroundTab.FirstPaint";
const char kHistogramSessionRestoreForegroundTabFirstContentfulPaint[] =
    "TabManager.Experimental.SessionRestore.ForegroundTab.FirstContentfulPaint";
const char kHistogramSessionRestoreForegroundTabFirstMeaningfulPaint[] =
    "TabManager.Experimental.SessionRestore.ForegroundTab.FirstMeaningfulPaint";

}  // namespace internal

SessionRestorePageLoadMetricsObserver::SessionRestorePageLoadMetricsObserver() {
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SessionRestorePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  content::WebContents* contents = navigation_handle->GetWebContents();
  if (!started_in_foreground ||
      !resource_coordinator::TabManager::IsTabInSessionRestore(contents) ||
      !resource_coordinator::TabManager::IsTabRestoredInForeground(contents)) {
    return STOP_OBSERVING;
  }

  // The navigation should be from the last session.
  DCHECK(navigation_handle->GetRestoreType() ==
             content::RestoreType::LAST_SESSION_EXITED_CLEANLY ||
         navigation_handle->GetRestoreType() ==
             content::RestoreType::LAST_SESSION_CRASHED);

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SessionRestorePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // Session restores use transition reload, so we only observe loads with a
  // reload transition type.
  DCHECK(ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                      ui::PAGE_TRANSITION_RELOAD));
  return CONTINUE_OBSERVING;
}

void SessionRestorePageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint.value(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSessionRestoreForegroundTabFirstPaint,
        timing.paint_timing->first_paint.value());

    // Only record the corresponding tab count if there are paint metrics. There
    // is no need to record again in FCP or FMP, because FP comes first.
    ukm::builders::
        TabManager_Experimental_SessionRestore_ForegroundTab_PageLoad(
            GetDelegate().GetSourceId())
            .SetSessionRestoreTabCount(
                g_browser_process->GetTabManager()->restored_tab_count())
            .SetSystemTabCount(
                g_browser_process->GetTabManager()->GetTabCount())
            .Record(ukm::UkmRecorder::Get());
  }
}

void SessionRestorePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint.value(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSessionRestoreForegroundTabFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }
}

void SessionRestorePageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint.value(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSessionRestoreForegroundTabFirstMeaningfulPaint,
        timing.paint_timing->first_meaningful_paint.value());
  }
}
