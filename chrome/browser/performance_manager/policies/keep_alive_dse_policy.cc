// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/keep_alive_dse_policy.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::policies {

namespace {

TemplateURLService* GetTemplateURLService(const PageNode* page_node) {
  content::WebContents* web_contents = page_node->GetWebContents().get();
  CHECK(web_contents);

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  CHECK(browser_context);

  return TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

}  // namespace

KeepAliveDSEPolicy::KeepAliveDSEPolicy() = default;

KeepAliveDSEPolicy::~KeepAliveDSEPolicy() = default;

void KeepAliveDSEPolicy::OnMainFrameUrlChanged(const PageNode* page_node) {
  const FrameNode* main_frame_node = page_node->GetMainFrameNode();
  // If there's no main frame (e.g., this can happen during session restore
  // before the main frame is fully initialized), there's nothing to do, as we
  // need it to determine the URL and associated process.
  if (!main_frame_node) {
    return;
  }

  TemplateURLService* template_url_service = GetTemplateURLService(page_node);

  // If the TemplateURLService can't be retrieved (e.g., for an off-the-record
  // profile or a profile that doesn't have one), we cannot determine the DSE
  // or if the current page is a DSE SRP.
  if (!template_url_service) {
    return;
  }

  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();

  // Only proceed if this is a search results page from a valid DSE.
  if (!default_search_provider ||
      !template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          page_node->GetMainFrameUrl())) {
    return;
  }

  // Since `page_node` is a search results page, we need to ensure we're keeping
  // its renderer alive, if we're not already.
  if (current_dse_id_) {
    if (*current_dse_id_ == default_search_provider->id()) {
      // Already keeping it alive. Bail out.
      return;
    }

    // We are keeping alive the wrong DSE. Release the existing keep-alive.
    ReleaseDSEKeepAlive();
  }

  SetDSEKeepAlive(page_node->GetMainFrameNode()->GetProcessNode(),
                  template_url_service);
}

void KeepAliveDSEPolicy::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  if (dse_renderer_kept_alive_ == process_node) {
    // The process we were keeping alive is being removed. We must release our
    // reference to it and clean up our internal state.
    //
    // We do NOT call DecrementPendingReuseRefCount() here. The process is
    // already being torn down, so the ref count is moot. Calling it during this
    // notification can lead to a crash if the shutdown was initiated by
    // RenderProcessHostImpl::DisableRefCounts(), which is what happens during
    // profile destruction in tests.
    template_url_service_observation_.Reset();
    dse_renderer_kept_alive_ = nullptr;
    current_dse_id_.reset();
  }
}

void KeepAliveDSEPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
  graph_ = graph;
}

void KeepAliveDSEPolicy::OnTakenFromGraph(Graph* graph) {
  CHECK(!dse_renderer_kept_alive_);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
  graph_ = nullptr;
}

void KeepAliveDSEPolicy::OnTemplateURLServiceChanged() {
  const TemplateURL* default_search_provider =
      template_url_service_observation_.GetSource()->GetDefaultSearchProvider();

  // The DSE has been disabled entirely (e.g., by enterprise policy).
  // If we are keeping a renderer alive, we must release it.
  if (!default_search_provider) {
    if (dse_renderer_kept_alive_) {
      ReleaseDSEKeepAlive();
    }
    return;
  }

  // If the default search provider hasn't changed, there's no need to update
  // the currently kept-alive renderer. However, a new DSE renderer might need
  // to be found and kept alive if one isn't already.
  if (current_dse_id_ && *current_dse_id_ == default_search_provider->id()) {
    // The DSE hasn't changed.
    return;
  }

  // The DSE has changed to a new, valid provider.
  // Release the old one (if any) and find a process for the new one.
  if (dse_renderer_kept_alive_) {
    ReleaseDSEKeepAlive();
  }
  FindAndKeepAliveDSERenderer();
}

void KeepAliveDSEPolicy::OnTemplateURLServiceShuttingDown() {
  ReleaseDSEKeepAlive();
}

void KeepAliveDSEPolicy::FindAndKeepAliveDSERenderer() {
  const PageNode* suitable_page = FindSuitableDSEPage();
  if (!suitable_page) {
    return;
  }

  KeepAliveDSERendererForPage(suitable_page);
}

// Helper function to find a suitable DSE page. Iterates through all pages
// in the graph and returns the first page that is considered suitable for
// DSE keep-alive, as determined by IsSuitableDSEPage. Returns nullptr if no
// suitable page is found.
const PageNode* KeepAliveDSEPolicy::FindSuitableDSEPage() const {
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    if (IsSuitableDSEPage(page_node)) {
      return page_node;
    }
  }
  return nullptr;
}

// Helper function to keep the DSE renderer alive for a given page.
// Retrieves the associated ProcessNode and TemplateURLService and then calls
// SetDSEKeepAlive to perform the actual keep-alive operation.
void KeepAliveDSEPolicy::KeepAliveDSERendererForPage(
    const PageNode* page_node) {
  const ProcessNode* process_node =
      page_node->GetMainFrameNode()->GetProcessNode();
  CHECK(process_node);

  TemplateURLService* template_url_service = GetTemplateURLService(page_node);

  SetDSEKeepAlive(process_node, template_url_service);
}

void KeepAliveDSEPolicy::SetDSEKeepAlive(
    const ProcessNode* process_node,
    TemplateURLService* template_url_service) {
  CHECK(process_node);
  CHECK(!dse_renderer_kept_alive_);
  CHECK(template_url_service);

  dse_renderer_kept_alive_ = process_node;

  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  CHECK(default_search_provider);

  current_dse_id_ = default_search_provider->id();

  // Start observing the Template URL service.
  template_url_service_observation_.Observe(template_url_service);

  content::RenderProcessHost* render_process_host =
      dse_renderer_kept_alive_->GetRenderProcessHostProxy().Get();
  CHECK(render_process_host);

  // Increment the ref count to keep the renderer alive.
  render_process_host->IncrementPendingReuseRefCount();
}

void KeepAliveDSEPolicy::ReleaseDSEKeepAlive() {
  // Immediately stop observing the TemplateURLService.
  template_url_service_observation_.Reset();

  CHECK(dse_renderer_kept_alive_);
  content::RenderProcessHost* render_process_host =
      dse_renderer_kept_alive_->GetRenderProcessHostProxy().Get();

  CHECK(render_process_host);
  render_process_host->DecrementPendingReuseRefCount();

  dse_renderer_kept_alive_ = nullptr;
  current_dse_id_.reset();
}

// Checks if a given PageNode represents a page that is suitable for
// keeping the Default Search Engine (DSE) renderer alive.
// A page is considered suitable if it meets the following criteria:
// 1. It has a main frame node. (This also guarantees a process node and
//    a RenderProcessHost).
// 2. The main frame's URL is identified as a search results page from the
//    default search provider by the TemplateURLService.
bool KeepAliveDSEPolicy::IsSuitableDSEPage(const PageNode* page_node) const {
  if (!page_node->GetMainFrameNode()) {
    return false;
  }

  TemplateURLService* template_url_service = GetTemplateURLService(page_node);
  // A TemplateURLService isn't available for all profile types (e.g., Guest)
  // or during profile teardown. We must check for null before using it.
  if (!template_url_service) {
    return false;
  }

  return template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
      page_node->GetMainFrameUrl());
}

}  // namespace performance_manager::policies
