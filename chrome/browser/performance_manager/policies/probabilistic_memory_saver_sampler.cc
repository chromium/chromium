// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/probabilistic_memory_saver_sampler.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

ProbabilisticMemorySaverSampler::ProbabilisticMemorySaverSampler(Graph* graph)
    : graph_(graph) {
  static const base::TimeDelta sampling_interval =
      features::kProactiveDiscardingSamplingInterval.Get();
  timer_.Start(FROM_HERE, sampling_interval, this,
               &ProbabilisticMemorySaverSampler::OnTimerElapsed);

  TabPageDecorator* tab_page_decorator =
      graph_->GetRegisteredObjectAs<TabPageDecorator>();
  CHECK(tab_page_decorator);
  tab_page_decorator->AddObserver(this);
}

ProbabilisticMemorySaverSampler::~ProbabilisticMemorySaverSampler() {
  TabPageDecorator* tab_page_decorator =
      graph_->GetRegisteredObjectAs<TabPageDecorator>();
  if (tab_page_decorator) {
    tab_page_decorator->RemoveObserver(this);
  }
}

void ProbabilisticMemorySaverSampler::OnTabAdded(
    TabPageDecorator::TabHandle* tab_handle) {
  auto inserted = tabs_.insert(tab_handle);
  CHECK(inserted.second);
}

void ProbabilisticMemorySaverSampler::OnBeforeTabRemoved(
    TabPageDecorator::TabHandle* tab_handle) {
  size_t num_erased = tabs_.erase(tab_handle);
  CHECK_EQ(num_erased, 1UL);
}

void ProbabilisticMemorySaverSampler::OnTimerElapsed() {
  for (auto* tab : tabs_) {
    PageLiveStateDecorator::Data* live_state_data =
        PageLiveStateDecorator::Data::GetOrCreateForPageNode(tab->page_node());
    // Don't need to sample the tab if it's the active tab in its window.
    if (!live_state_data->IsActiveTab()) {
      Sample(tab);
    }
  }
}

}  // namespace performance_manager
