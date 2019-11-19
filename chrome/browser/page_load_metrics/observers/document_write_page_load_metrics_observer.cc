// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

namespace internal {
const char kHistogramDocWriteBlockFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramDocWriteBlockParseBlockedOnScriptExecution[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptExecution";
const char kHistogramDocWriteBlockParseBlockedOnScriptExecutionDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming."
    "ParseBlockedOnScriptExecutionFromDocumentWrite";
const char kHistogramDocWriteBlockParseDuration[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseDuration";

const char kBackgroundHistogramDocWriteBlockParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad."
    "Background";
const char kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite.Background";
const char kBackgroundHistogramDocWriteBlockParseDuration[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseDuration.Background";

const char kHistogramDocWriteBlockCount[] =
    "PageLoad.Clients.DocWrite.Block.Count";
const char kHistogramDocWriteBlockReloadCount[] =
    "PageLoad.Clients.DocWrite.Block.ReloadCount";
const char kHistogramDocWriteBlockLoadingBehavior[] =
    "PageLoad.Clients.DocWrite.Block.DocumentWriteLoadingBehavior";

}  // namespace internal

void DocumentWritePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetMainFrameMetadata().behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockFirstContentfulPaint(timing);
  }
}

void DocumentWritePageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetMainFrameMetadata().behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockFirstMeaningfulPaint(timing);
  }
}

void DocumentWritePageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetMainFrameMetadata().behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockParseStop(timing);
  }
}

// static
void DocumentWritePageLoadMetricsObserver::LogLoadingBehaviorMetrics(
    DocumentWritePageLoadMetricsObserver::DocumentWriteLoadingBehavior behavior,
    ukm::SourceId source_id) {
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramDocWriteBlockLoadingBehavior, behavior,
      DocumentWritePageLoadMetricsObserver::LOADING_BEHAVIOR_MAX);

  // We only log the block and reload behaviors in UKM.
  if (behavior != LOADING_BEHAVIOR_BLOCK &&
      behavior != LOADING_BEHAVIOR_RELOAD) {
    return;
  }
  ukm::builders::Intervention_DocumentWrite_ScriptBlock builder(source_id);
  if (behavior == LOADING_BEHAVIOR_RELOAD)
    builder.SetDisabled_Reload(true);
  builder.Record(ukm::UkmRecorder::Get());
}

void DocumentWritePageLoadMetricsObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flags) {
  if ((GetDelegate().GetMainFrameMetadata().behavior_flags &
       blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlockReload) &&
      !doc_write_block_reload_observed_) {
    DCHECK(!(GetDelegate().GetMainFrameMetadata().behavior_flags &
             blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock));
    UMA_HISTOGRAM_COUNTS_1M(internal::kHistogramDocWriteBlockReloadCount, 1);
    LogLoadingBehaviorMetrics(LOADING_BEHAVIOR_RELOAD,
                              GetDelegate().GetSourceId());
    doc_write_block_reload_observed_ = true;
  }
  if ((GetDelegate().GetMainFrameMetadata().behavior_flags &
       blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock) &&
      !doc_write_block_observed_) {
    UMA_HISTOGRAM_BOOLEAN(internal::kHistogramDocWriteBlockCount, true);
    LogLoadingBehaviorMetrics(LOADING_BEHAVIOR_BLOCK,
                              GetDelegate().GetSourceId());
    doc_write_block_observed_ = true;
  }
  if ((GetDelegate().GetMainFrameMetadata().behavior_flags &
       blink::LoadingBehaviorFlag::
           kLoadingBehaviorDocumentWriteBlockDifferentScheme) &&
      !doc_write_same_site_diff_scheme_) {
    LogLoadingBehaviorMetrics(LOADING_BEHAVIOR_SAME_SITE_DIFF_SCHEME,
                              GetDelegate().GetSourceId());
    doc_write_same_site_diff_scheme_ = true;
  }
}

// Note: The first meaningful paint calculation in the core observer filters
// out pages which had user interaction before the first meaningful paint.
// Because the counts of those instances are low (< 2%), just log everything
// here for simplicity. If this ends up being unreliable (the 2% is just from
// canary), the page_load_metrics API should be altered to return the values
// the consumer wants.
void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteBlockFirstMeaningfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.DocWrite.Block.Experimental.PaintTiming."
        "ParseStartToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteBlockFirstContentfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteBlockParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptExecution,
        timing.parse_timing->parse_blocked_on_script_execution_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramDocWriteBlockParseBlockedOnScriptExecutionDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_execution_from_document_write_duration
            .value());

    ukm::builders::Intervention_DocumentWrite_ScriptBlock(
        GetDelegate().GetSourceId())
        .SetParseTiming_ParseBlockedOnScriptLoadFromDocumentWrite(
            timing.parse_timing
                ->parse_blocked_on_script_load_from_document_write_duration
                ->InMilliseconds())
        .SetParseTiming_ParseBlockedOnScriptExecutionFromDocumentWrite(
            timing.parse_timing
                ->parse_blocked_on_script_execution_from_document_write_duration
                ->InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseDuration,
        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}
