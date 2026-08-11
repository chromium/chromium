// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/execution_context_priority/extension_service_worker_priority_voter.h"

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const WorkerNode* worker_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             worker_node->GetGraph())
      ->GetExecutionContextForWorkerNode(worker_node);
}

// Returns true if `worker_node` is the service worker of an extension that
// holds the `webRequestBlocking` permission.
//
// `pending_process_node` must be used instead of
// `worker_node->GetProcessNode()` because the latter is always null while the
// node is being added to the graph.
bool IsBlockingExtensionServiceWorker(const WorkerNode* worker_node,
                                      const ProcessNode* pending_process_node) {
  if (worker_node->GetWorkerType() != WorkerNode::WorkerType::kService) {
    return false;
  }

  const url::Origin& origin = worker_node->GetOrigin();
  if (origin.scheme() != extensions::kExtensionScheme) {
    return false;
  }

  if (!pending_process_node || pending_process_node->GetProcessType() !=
                                   content::PROCESS_TYPE_RENDERER) {
    return false;
  }

  // Safe to resolve the proxy because the performance manager graph runs on the
  // UI thread. May still be null if the host is already gone.
  content::RenderProcessHost* render_process_host =
      pending_process_node->GetRenderProcessHostProxy().Get();
  if (!render_process_host) {
    return false;
  }

  extensions::ExtensionRegistry* registry = extensions::ExtensionRegistry::Get(
      render_process_host->GetBrowserContext());
  if (!registry) {
    return false;
  }

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(origin.host());
  if (!extension) {
    return false;
  }

  // TODO(crbug.com/484218883): Permissions can change at runtime (e.g. an
  // optional permission being granted), which is not reflected in the vote
  // submitted here. Extend this to observe permission changes if telemetry
  // shows it matters in practice.
  return extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kWebRequestBlocking);
}

}  // namespace

const char ExtensionServiceWorkerPriorityVoter::kPriorityReason[] =
    "Extension service worker with blocking webRequest.";

ExtensionServiceWorkerPriorityVoter::ExtensionServiceWorkerPriorityVoter() =
    default;
ExtensionServiceWorkerPriorityVoter::~ExtensionServiceWorkerPriorityVoter() =
    default;

void ExtensionServiceWorkerPriorityVoter::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddWorkerNodeObserver(this);
}

void ExtensionServiceWorkerPriorityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveWorkerNodeObserver(this);
  voting_channel_.Reset();
}

void ExtensionServiceWorkerPriorityVoter::OnBeforeWorkerNodeAdded(
    const WorkerNode* worker_node,
    const ProcessNode* pending_process_node) {
  const base::Process::Priority priority =
      IsBlockingExtensionServiceWorker(worker_node, pending_process_node)
          ? base::Process::Priority::kUserBlocking
          : base::Process::Priority::kMinValue;
  voting_channel_.SubmitVote(GetExecutionContext(worker_node),
                             Vote(priority, kPriorityReason));
}

void ExtensionServiceWorkerPriorityVoter::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(worker_node));
}

}  // namespace performance_manager::execution_context_priority
