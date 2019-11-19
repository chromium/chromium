// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"

#include <algorithm>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

// Provides PageAlmostIdle machinery access to some internals of a PageNodeImpl.
class PageAlmostIdleAccess {
 public:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return &page_node->page_almost_idle_data_;
  }

  static void SetPageAlmostIdle(PageNodeImpl* page_node,
                                bool page_almost_idle) {
    page_node->SetPageAlmostIdle(page_almost_idle);
  }
};

namespace {

using LoadIdleState = PageAlmostIdleDecorator::Data::LoadIdleState;

class DataImpl : public PageAlmostIdleDecorator::Data,
                 public NodeAttachedDataImpl<DataImpl> {
 public:
  struct Traits : public NodeAttachedDataOwnedByNodeType<PageNodeImpl> {};

  explicit DataImpl(const PageNodeImpl* page_node) {}
  ~DataImpl() override = default;

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return PageAlmostIdleAccess::GetUniquePtrStorage(page_node);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DataImpl);
};

}  // namespace

// static
constexpr base::TimeDelta PageAlmostIdleDecorator::kLoadedAndIdlingTimeout;
// static
constexpr base::TimeDelta PageAlmostIdleDecorator::kWaitingForIdleTimeout;

PageAlmostIdleDecorator::PageAlmostIdleDecorator() {
  // Ensure the timeouts make sense relative to each other.
  static_assert(kWaitingForIdleTimeout > kLoadedAndIdlingTimeout,
                "timeouts must be well ordered");
}

PageAlmostIdleDecorator::~PageAlmostIdleDecorator() = default;

void PageAlmostIdleDecorator::OnNetworkAlmostIdleChanged(
    const FrameNode* frame_node) {
  UpdateLoadIdleStateFrame(FrameNodeImpl::FromNode(frame_node));
}

void PageAlmostIdleDecorator::OnPassedToGraph(Graph* graph) {
  RegisterObservers(graph);
}

void PageAlmostIdleDecorator::OnTakenFromGraph(Graph* graph) {
  UnregisterObservers(graph);
}

void PageAlmostIdleDecorator::OnIsLoadingChanged(const PageNode* page_node) {
  UpdateLoadIdleStatePage(PageNodeImpl::FromNode(page_node));
}

void PageAlmostIdleDecorator::OnMainFrameDocumentChanged(
    const PageNode* page_node) {
  DCHECK_NE(0, page_node->GetNavigationID());
  // Reset the load-idle state associated with this page as a new navigation has
  // started.
  auto* page_impl = PageNodeImpl::FromNode(page_node);
  auto* data = DataImpl::GetOrCreate(page_impl);
  data->load_idle_state_ = LoadIdleState::kLoadingNotStarted;
  PageAlmostIdleAccess::SetPageAlmostIdle(page_impl, false);
  UpdateLoadIdleStatePage(page_impl);
}

void PageAlmostIdleDecorator::OnMainThreadTaskLoadIsLow(
    const ProcessNode* process_node) {
  UpdateLoadIdleStateProcess(ProcessNodeImpl::FromNode(process_node));
}

void PageAlmostIdleDecorator::RegisterObservers(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  // TODO(chrisha): Add graph introspection functions to Graph.
  DCHECK(GraphImpl::FromGraph(graph)->nodes().empty());
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void PageAlmostIdleDecorator::UnregisterObservers(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

void PageAlmostIdleDecorator::UpdateLoadIdleStateFrame(
    FrameNodeImpl* frame_node) {
  // Only main frames are relevant in the load idle state.
  if (!frame_node->IsMainFrame())
    return;

  // Update the load idle state of the page associated with this frame.
  auto* page_node = frame_node->page_node();
  if (!page_node)
    return;
  UpdateLoadIdleStatePage(page_node);
}

void PageAlmostIdleDecorator::UpdateLoadIdleStatePage(PageNodeImpl* page_node) {
  // Once the cycle is complete state transitions are no longer tracked for this
  // page. When this occurs the backing data store is deleted.
  auto* data = DataImpl::Get(page_node);
  if (data == nullptr)
    return;

  // This is the terminal state, so should never occur.
  DCHECK_NE(LoadIdleState::kLoadedAndIdle, data->load_idle_state_);

  // Cancel any ongoing timers. A new timer will be set if necessary.
  data->idling_timer_.Stop();
  const base::TimeTicks now = base::TimeTicks::Now();

  // Determine if the overall timeout has fired.
  if ((data->load_idle_state_ == LoadIdleState::kLoadedNotIdling ||
       data->load_idle_state_ == LoadIdleState::kLoadedAndIdling) &&
      (now - data->loading_stopped_) >= kWaitingForIdleTimeout) {
    TransitionToLoadedAndIdle(page_node);
    return;
  }

  // Otherwise do normal state transitions.
  switch (data->load_idle_state_) {
    case LoadIdleState::kLoadingNotStarted: {
      if (!page_node->is_loading())
        return;
      data->load_idle_state_ = LoadIdleState::kLoading;
      return;
    }

    case LoadIdleState::kLoading: {
      if (page_node->is_loading())
        return;
      data->load_idle_state_ = LoadIdleState::kLoadedNotIdling;
      data->loading_stopped_ = now;
      // Let the kLoadedNotIdling state transition evaluate, allowing an
      // effective transition directly from kLoading to kLoadedAndIdling.
      FALLTHROUGH;
    }

    case LoadIdleState::kLoadedNotIdling: {
      if (IsIdling(page_node)) {
        data->load_idle_state_ = LoadIdleState::kLoadedAndIdling;
        data->idling_started_ = now;
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    case LoadIdleState::kLoadedAndIdling: {
      // If the page is not still idling then transition back a state.
      if (!IsIdling(page_node)) {
        data->load_idle_state_ = LoadIdleState::kLoadedNotIdling;
      } else {
        // Idling has been happening long enough so make the last state
        // transition.
        if (now - data->idling_started_ >= kLoadedAndIdlingTimeout) {
          TransitionToLoadedAndIdle(page_node);
          return;
        }
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    // This should never occur.
    case LoadIdleState::kLoadedAndIdle:
      NOTREACHED();
  }

  // Getting here means a new timer needs to be set. Use the nearer of the two
  // applicable timeouts.
  base::TimeDelta timeout =
      (data->loading_stopped_ + kWaitingForIdleTimeout) - now;
  if (data->load_idle_state_ == LoadIdleState::kLoadedAndIdling) {
    timeout = std::min(timeout,
                       (data->idling_started_ + kLoadedAndIdlingTimeout) - now);
  }

  // It's safe to use base::Unretained here because the graph owns the timer via
  // PageNodeImpl, and all nodes are destroyed *before* this observer during
  // tear down. By the time the observer is destroyed, the timer will have
  // already been destroyed and the associated posted task canceled.
  data->idling_timer_.Start(
      FROM_HERE, timeout,
      base::BindRepeating(&PageAlmostIdleDecorator::UpdateLoadIdleStatePage,
                          base::Unretained(this), page_node));
}

void PageAlmostIdleDecorator::UpdateLoadIdleStateProcess(
    ProcessNodeImpl* process_node) {
  for (auto* frame_node : process_node->frame_nodes())
    UpdateLoadIdleStateFrame(frame_node);
}

void PageAlmostIdleDecorator::TransitionToLoadedAndIdle(
    PageNodeImpl* page_node) {
  auto* data = DataImpl::Get(page_node);
  data->load_idle_state_ = LoadIdleState::kLoadedAndIdle;
  PageAlmostIdleAccess::SetPageAlmostIdle(page_node, true);

  // Destroy the metadata as there are no more transitions possible. The
  // machinery will start up again if a navigation occurs.
  DataImpl::Destroy(page_node);
}

// static
bool PageAlmostIdleDecorator::IsIdling(const PageNodeImpl* page_node) {
  // Get the frame node for the main frame associated with this page.
  const FrameNodeImpl* main_frame_node = page_node->GetMainFrameNodeImpl();
  if (!main_frame_node)
    return false;

  // Get the process node associated with this main frame.
  const auto* process_node = main_frame_node->process_node();
  if (!process_node)
    return false;

  // Note that it's possible for one misbehaving frame hosted in the same
  // process as this page's main frame to keep the main thread task low high.
  // In this case the IsIdling signal will be delayed, despite the task load
  // associated with this page's main frame actually being low. In the case
  // of session restore this is mitigated by having a timeout while waiting for
  // this signal.
  return main_frame_node->network_almost_idle() &&
         process_node->main_thread_task_load_is_low();
}

// static
PageAlmostIdleDecorator::Data*
PageAlmostIdleDecorator::Data::GetOrCreateForTesting(PageNodeImpl* page_node) {
  return DataImpl::GetOrCreate(page_node);
}

// static
PageAlmostIdleDecorator::Data* PageAlmostIdleDecorator::Data::GetForTesting(
    PageNodeImpl* page_node) {
  return DataImpl::Get(page_node);
}

// static
bool PageAlmostIdleDecorator::Data::DestroyForTesting(PageNodeImpl* page_node) {
  return DataImpl::Destroy(page_node);
}

}  // namespace performance_manager
