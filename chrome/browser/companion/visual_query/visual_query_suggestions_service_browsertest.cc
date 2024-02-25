// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/visual_query/visual_query_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/companion/visual_query/features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class VisualQuerySuggestionsServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  VisualQuerySuggestionsServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        companion::visual_query::features::kVisualQuerySuggestions);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    background_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    test_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider_;
};

IN_PROC_BROWSER_TEST_F(VisualQuerySuggestionsServiceDisabledBrowserTest,
                       VisualQuerySuggestionsServiceDisabled) {
  EXPECT_FALSE(companion::visual_query::VisualQuerySuggestionsServiceFactory::
                   GetForProfile(browser()->profile()));
}

class VisualQuerySuggestionsServiceBrowserTest
    : public VisualQuerySuggestionsServiceDisabledBrowserTest {
 public:
  VisualQuerySuggestionsServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {companion::visual_query::features::kVisualQuerySuggestions}, {});
  }

  companion::visual_query::VisualQuerySuggestionsService*
  visual_query_suggestions_service() {
    return companion::visual_query::VisualQuerySuggestionsServiceFactory::
        GetForProfile(browser()->profile());
  }

  ~VisualQuerySuggestionsServiceBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VisualQuerySuggestionsServiceBrowserTest,
                       VisualQuerySuggestionsServiceEnabled) {
  EXPECT_TRUE(visual_query_suggestions_service());
}

}  // namespace
