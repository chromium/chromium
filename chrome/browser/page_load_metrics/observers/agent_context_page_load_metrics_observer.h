// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AGENT_CONTEXT_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AGENT_CONTEXT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Records, once per page load, whether an actor task (e.g. the Glic agent) was
// acting on the tab and whether a DevTools client was attached to it when the
// page committed. A prerendered page is recorded when it is activated instead.
class AgentContextPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(AgentContext)
  enum class AgentContext {
    kNone = 0,
    kActorTask = 1,
    kDevToolsClient = 2,
    kActorTaskAndDevToolsClient = 3,
    kMaxValue = kActorTaskAndDevToolsClient,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PageLoadAgentContext)

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AGENT_CONTEXT_PAGE_LOAD_METRICS_OBSERVER_H_
