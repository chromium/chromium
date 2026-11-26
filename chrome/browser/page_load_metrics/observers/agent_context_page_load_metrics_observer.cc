// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/agent_context_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/actor/actor_util.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"

namespace {

using AgentContext = AgentContextPageLoadMetricsObserver::AgentContext;

AgentContext GetAgentContext(content::WebContents* web_contents) {
  const bool has_actor_task = actor::HaveActiveTaskForContents(web_contents);
  // True for a client attached to either the page target or the tab target.
  const bool has_devtools_client =
      content::DevToolsAgentHost::IsDebuggerAttached(web_contents);
  if (has_actor_task && has_devtools_client) {
    return AgentContext::kActorTaskAndDevToolsClient;
  }
  if (has_actor_task) {
    return AgentContext::kActorTask;
  }
  if (has_devtools_client) {
    return AgentContext::kDevToolsClient;
  }
  return AgentContext::kNone;
}

void RecordAgentContext(content::NavigationHandle* navigation_handle) {
  base::UmaHistogramEnumeration(
      "PageLoad.Clients.AgentContextAtCommit",
      GetAgentContext(navigation_handle->GetWebContents()));
}

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AgentContextPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // A fenced frame is part of its embedder's page load, not one of its own.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AgentContextPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AgentContextPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // A prerendered page commits while hidden and may never be shown. It is
  // recorded in DidActivatePrerenderedPage() instead.
  if (navigation_handle->IsInPrerenderedMainFrame()) {
    return CONTINUE_OBSERVING;
  }
  RecordAgentContext(navigation_handle);
  return STOP_OBSERVING;
}

void AgentContextPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  RecordAgentContext(navigation_handle);
}
