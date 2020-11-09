// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/page_aggregator.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace testing {

void CreatePageAggregatorAndPassItToGraph() {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&](Graph* graph) {
        graph->PassToGraph(
            std::make_unique<performance_manager::PageAggregator>());
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
}

}  // namespace testing
}  // namespace performance_manager
