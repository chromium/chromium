// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILISTIC_MEMORY_SAVER_SAMPLER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILISTIC_MEMORY_SAVER_SAMPLER_H_

#include <set>

#include "base/timer/timer.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/user_tuning/proactive_discard_evaluator.h"

namespace performance_manager {

class Graph;

class ProbabilisticMemorySaverSampler
    : public ProactiveDiscardEvaluator::Sampler,
      public TabPageObserverDefaultImpl {
 public:
  explicit ProbabilisticMemorySaverSampler(Graph* graph);
  ~ProbabilisticMemorySaverSampler() override;

  // TabPageObserverDefaultImpl:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

 private:
  void OnTimerElapsed();

  std::set<TabPageDecorator::TabHandle*> tabs_;
  base::RepeatingTimer timer_;
  raw_ptr<Graph> graph_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILISTIC_MEMORY_SAVER_SAMPLER_H_
