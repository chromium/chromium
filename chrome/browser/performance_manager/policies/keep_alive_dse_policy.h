// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_KEEP_ALIVE_DSE_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_KEEP_ALIVE_DSE_POLICY_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

namespace performance_manager::policies {

// KeepAliveDSEPolicy is a policy which will force the renderer process of the
// default search engine to be kept alive after initialization.
class KeepAliveDSEPolicy : public PageNodeObserver,
                           public ProcessNodeObserver,
                           public GraphOwnedAndRegistered<KeepAliveDSEPolicy>,
                           public TemplateURLServiceObserver {
 public:
  KeepAliveDSEPolicy();
  ~KeepAliveDSEPolicy() override;
  KeepAliveDSEPolicy(const KeepAliveDSEPolicy&) = delete;
  KeepAliveDSEPolicy& operator=(const KeepAliveDSEPolicy&) = delete;

  // PageNodeObserver:
  void OnMainFrameUrlChanged(const PageNode* page_node) override;

  // ProcessNodeObserver:
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

 private:
  // Iterates through all existing pages and attempts to find a renderer
  // associated with the current default search engine. If found, keeps it
  // alive. This is called after a DSE change to ensure a renderer is kept
  // alive even if the user hasn't navigated to the new DSE yet.
  //
  // Note: Iterating through all pages could be expensive if there are many
  // pages. However, DSE changes are infrequent, and we bail out as soon as
  // a suitable renderer is found.
  void FindAndKeepAliveDSERenderer();

  // Helper function to find a suitable DSE page.
  const PageNode* FindSuitableDSEPage() const;

  // Helper function to keep the DSE renderer alive for a given page.
  void KeepAliveDSERendererForPage(const PageNode* page_node);

  // Marks the given process node as the one to be kept alive for the DSE,
  // and starts observing the associated TemplateURLService.
  // `template_url_service` must not be null and should belong to the
  // BrowserContext associated with `process_node`.
  void SetDSEKeepAlive(const ProcessNode* process_node,
                       TemplateURLService* template_url_service);

  // Releases the currently kept-alive DSE renderer and stops observing the
  // TemplateURLService.
  void ReleaseDSEKeepAlive();

  // Returns true if the given PageNode represents a suitable DSE page for
  // keep-alive.
  bool IsSuitableDSEPage(const PageNode* page_node) const;

  // The Graph that owns this policy.
  raw_ptr<Graph> graph_ = nullptr;

  raw_ptr<const ProcessNode> dse_renderer_kept_alive_ = nullptr;

  // The ID of the current default search engine.
  std::optional<TemplateURLID> current_dse_id_;

  // Observes the TemplateURLService for changes to the default search engine.
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_KEEP_ALIVE_DSE_POLICY_H_
