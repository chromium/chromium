// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace performance_manager::user_tuning {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);

class QuitRunLoopOnMemoryMetricsRefreshObserver
    : public UserPerformanceTuningManager::Observer {
 public:
  explicit QuitRunLoopOnMemoryMetricsRefreshObserver(
      base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~QuitRunLoopOnMemoryMetricsRefreshObserver() override = default;

  void OnMemoryMetricsRefreshed() override { std::move(quit_closure_).Run(); }

 private:
  base::OnceClosure quit_closure_;
};
}  // namespace

class ResourceUsageTabHelperTest : public InteractiveBrowserTest {
 public:
  ResourceUsageTabHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::kMemoryUsageInHovercards);
  }

  ~ResourceUsageTabHelperTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL(base::StringPiece path) {
    return embedded_test_server()->GetURL("example.com", path);
  }

  auto ForceRefreshMemoryMetrics() {
    return Do(base::BindLambdaForTesting([]() {
      UserPerformanceTuningManager* manager =
          UserPerformanceTuningManager::GetInstance();

      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      QuitRunLoopOnMemoryMetricsRefreshObserver observer(
          run_loop.QuitClosure());
      base::ScopedObservation<UserPerformanceTuningManager,
                              QuitRunLoopOnMemoryMetricsRefreshObserver>
          memory_metrics_observer(&observer);
      memory_metrics_observer.Observe(manager);

      performance_manager::PerformanceManager::CallOnGraph(
          FROM_HERE,
          base::BindLambdaForTesting([](performance_manager::Graph* graph) {
            auto* metrics_decorator = graph->GetRegisteredObjectAs<
                performance_manager::ProcessMetricsDecorator>();
            metrics_decorator->RequestImmediateMetrics();
          }));

      run_loop.Run();
    }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ResourceUsageTabHelperTest, MemoryUsagePopulated) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      ForceRefreshMemoryMetrics(), Check(base::BindLambdaForTesting([=]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* resource_usage =
            performance_manager::user_tuning::UserPerformanceTuningManager::
                ResourceUsageTabHelper::FromWebContents(web_contents);
        return resource_usage && resource_usage->GetMemoryUsageInBytes() != 0;
      })));
}

}  // namespace performance_manager::user_tuning
