// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_observation_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/size.h"

namespace actor {

using ::optimization_guide::AIPageContentError;
using ::optimization_guide::AIPageContentExtractionStatus;
using ::optimization_guide::AIPageContentResult;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::TabObservation;
using ::page_content_annotations::FetchPageContextResult;
using ::page_content_annotations::ScreenshotCaptureStatus;
using ::page_content_annotations::ScreenshotError;
using ::page_content_annotations::ScreenshotResult;

class TabObservationControllerBrowserTest : public InProcessBrowserTest {
 public:
  TabObservationControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicActor}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  ActorKeyedService* actor_service() {
    return ActorKeyedService::Get(browser()->GetProfile());
  }

  TaskId CreateTask() {
    return actor_service()->CreateTask(TestTaskSourceInfo(),
                                       NoEnterprisePolicyChecker());
  }

  mojom::ActionResultPtr MakeScriptToolActionResult() {
    mojom::ActionResultPtr action_result = MakeOkResult();
    action_result->script_tool_response = mojom::ScriptToolResponse::New();
    action_result->script_tool_response->result = "script_tool_output";
    action_result->script_tool_response->input_arguments = "args";
    action_result->script_tool_response->tool = blink::mojom::ScriptTool::New();
    action_result->script_tool_response->tool->name = "test_tool";
    action_result->script_tool_response->tool->description = "desc";
    return action_result;
  }

  ScreenshotResult MakeScreenshotResult() {
    ScreenshotResult screenshot(gfx::Size(10, 10));
    screenshot.screenshot_data = {1, 2, 3};
    screenshot.mime_type = "image/jpeg";
    return screenshot;
  }

  void FetchObservationWithResult(
      std::unique_ptr<FetchPageContextResult> fetch_result,
      TabObservation& observation) {
    TaskId task_id = CreateTask();
    GURL url = embedded_test_server()->GetURL("/title1.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    tabs::TabInterface* tab = browser()->GetActiveTabInterface();
    actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

    TabObservationController controller(
        browser()->GetProfile(), task_id, base::TimeTicks::Now(),
        /*skip_async_observation_information=*/false, {},
        TabObservationStrategy(), base::DoNothing());

    observation.set_id(tab->GetHandle().raw_value());
    controller.OnTabObservationFetchedForTesting(
        ActorSurfaceHandle::From(tab->GetHandle()), &observation,
        base::ok(std::move(fetch_result)));
  }

  void RecordObservationHistogram(const TabObservation& observation) {
    ActionsResult actions_result;
    *actions_result.add_tabs() = observation;
    RecordTabObservationResultHistogram(actions_result);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest, ObserveSingleTab) {
  base::HistogramTester histogram_tester;
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {},
      TabObservationStrategy(), future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].id(), tab->GetHandle().raw_value());
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextApcStatus,
      optimization_guide::AIPageContentExtractionStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextScreenshotStatus,
      page_content_annotations::ScreenshotCaptureStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveMultipleTabs) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab1, *actor_service()->GetTask(task_id));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tabs::TabInterface* tab2 = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab2, *actor_service()->GetTask(task_id));

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {},
      TabObservationStrategy(), future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  EXPECT_EQ(result->tab_observations.size(), 2u);

  bool found_tab1 = false;
  bool found_tab2 = false;
  for (const auto& obs : result->tab_observations) {
    if (obs.id() == tab1->GetHandle().raw_value()) {
      found_tab1 = true;
    } else if (obs.id() == tab2->GetHandle().raw_value()) {
      found_tab2 = true;
    }
    EXPECT_EQ(obs.result(),
              optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  }
  EXPECT_TRUE(found_tab1);
  EXPECT_TRUE(found_tab2);
}

// TODO(crbug.com/513631749): Re-enable this test.
IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       DISABLED_ObserveCrashedTabWithReload) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  // Crash the tab.
  content::RenderProcessHost* process =
      tab->GetContents()->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  crash_observer.Wait();
  ASSERT_TRUE(tab->GetContents()->IsCrashed());

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {},
      TabObservationStrategy(), future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].id(), tab->GetHandle().raw_value());
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_FALSE(tab->GetContents()->IsCrashed());
}

class TabObservationControllerWithoutScreenshotBrowserTest
    : public TabObservationControllerBrowserTest {
 public:
  TabObservationControllerWithoutScreenshotBrowserTest() {
    screenshot_feature_list_.InitAndEnableFeature(kGlicActorSkipScreenshot);
  }

 private:
  base::test::ScopedFeatureList screenshot_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabObservationControllerWithoutScreenshotBrowserTest,
                       ObserveWithoutScreenshot) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {},
      TabObservationStrategy(), future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_screenshot());
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveWithOmittedScreenshot) {
  base::HistogramTester histogram_tester;
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(ActorSurfaceHandle::From(tab->GetHandle()),
                             ScreenshotPolicy::kSkipped);
  strategy.VoteForPageContentExtraction(
      ActorSurfaceHandle::From(tab->GetHandle()),
      PageContentExtractionPolicy::kRequested);

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, std::move(strategy),
      future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_screenshot());
  EXPECT_EQ(
      result->tab_observations[0].annotated_page_content_result(),
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_OK);
  EXPECT_TRUE(result->tab_observations[0].has_annotated_page_content());
  histogram_tester.ExpectUniqueSample(
      kActorPageContextApcStatus,
      optimization_guide::AIPageContentExtractionStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(kActorPageContextScreenshotStatus, 0);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveWithOmittedApc) {
  base::HistogramTester histogram_tester;
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(ActorSurfaceHandle::From(tab->GetHandle()),
                             ScreenshotPolicy::kRequested);
  strategy.VoteForPageContentExtraction(
      ActorSurfaceHandle::From(tab->GetHandle()),
      PageContentExtractionPolicy::kSkipped);

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, std::move(strategy),
      future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_TRUE(result->tab_observations[0].has_screenshot());
  EXPECT_EQ(
      result->tab_observations[0].annotated_page_content_result(),
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_annotated_page_content());
  histogram_tester.ExpectTotalCount(kActorPageContextApcStatus, 0);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextScreenshotStatus,
      page_content_annotations::ScreenshotCaptureStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveWithOmittedBoth) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(ActorSurfaceHandle::From(tab->GetHandle()),
                             ScreenshotPolicy::kSkipped);
  strategy.VoteForPageContentExtraction(
      ActorSurfaceHandle::From(tab->GetHandle()),
      PageContentExtractionPolicy::kSkipped);

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, std::move(strategy),
      future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_screenshot());
  EXPECT_EQ(
      result->tab_observations[0].annotated_page_content_result(),
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_annotated_page_content());
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveWithOmittedApcRetainsScriptToolResults) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(ActorSurfaceHandle::From(tab->GetHandle()),
                             ScreenshotPolicy::kRequested);
  strategy.VoteForPageContentExtraction(
      ActorSurfaceHandle::From(tab->GetHandle()),
      PageContentExtractionPolicy::kSkipped);

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false,
      MakeResultVector(MakeScriptToolActionResult()), std::move(strategy),
      future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(
      result->tab_observations[0].annotated_page_content_result(),
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_OK);

  ASSERT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results()
                .size(),
            1u);
  EXPECT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results(0)
                .result(),
            "script_tool_output");
  EXPECT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results(0)
                .tool_name(),
            "test_tool");
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveWithOmittedScreenshotRetainsScriptToolResults) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(ActorSurfaceHandle::From(tab->GetHandle()),
                             ScreenshotPolicy::kSkipped);
  strategy.VoteForPageContentExtraction(
      ActorSurfaceHandle::From(tab->GetHandle()),
      PageContentExtractionPolicy::kRequested);

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false,
      MakeResultVector(MakeScriptToolActionResult()), std::move(strategy),
      future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_screenshot());
  EXPECT_EQ(
      result->tab_observations[0].annotated_page_content_result(),
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_OK);

  ASSERT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results()
                .size(),
            1u);
  EXPECT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results(0)
                .result(),
            "script_tool_output");
  EXPECT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results(0)
                .tool_name(),
            "test_tool");
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveWithOmittedBothRetainsScriptToolResults) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  TabObservationStrategy strategy;
  strategy.VoteForScreenshot(ActorSurfaceHandle::From(tab->GetHandle()),
                             ScreenshotPolicy::kSkipped);
  strategy.VoteForPageContentExtraction(
      ActorSurfaceHandle::From(tab->GetHandle()),
      PageContentExtractionPolicy::kSkipped);

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->GetProfile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false,
      MakeResultVector(MakeScriptToolActionResult()), std::move(strategy),
      future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_screenshot());
  EXPECT_EQ(
      result->tab_observations[0].annotated_page_content_result(),
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_OK);

  ASSERT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results()
                .size(),
            1u);
  EXPECT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results(0)
                .result(),
            "script_tool_output");
  EXPECT_EQ(result->tab_observations[0]
                .annotated_page_content()
                .main_frame_data()
                .script_tool_results(0)
                .tool_name(),
            "test_tool");
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       OnTabObservationFetchedMapsScreenshotTimeout) {
  base::HistogramTester histogram_tester;
  auto fetch_result = std::make_unique<FetchPageContextResult>();
  fetch_result->annotated_page_content_result.emplace(AIPageContentResult());
  fetch_result->screenshot_result = base::unexpected(
      ScreenshotError{ScreenshotCaptureStatus::kTimeout, "timed out"});

  TabObservation observation;
  ASSERT_NO_FATAL_FAILURE(
      FetchObservationWithResult(std::move(fetch_result), observation));

  EXPECT_EQ(observation.result(), TabObservation::TAB_OBSERVATION_FETCH_ERROR);
  EXPECT_EQ(observation.annotated_page_content_result(),
            TabObservation::ANNOTATED_PAGE_CONTENT_OK);
  EXPECT_TRUE(observation.has_annotated_page_content());
  EXPECT_EQ(observation.screenshot_result(),
            TabObservation::SCREENSHOT_TIMEOUT);
  EXPECT_FALSE(observation.has_screenshot());

  histogram_tester.ExpectUniqueSample(
      kActorPageContextApcStatus, AIPageContentExtractionStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(kActorPageContextScreenshotStatus,
                                      ScreenshotCaptureStatus::kTimeout, 1);

  RecordObservationHistogram(observation);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextTabObservationResult,
      ActorTabObservationResult::kScreenshotTimeout, 1);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       OnTabObservationFetchedMapsScreenshotError) {
  base::HistogramTester histogram_tester;
  auto fetch_result = std::make_unique<FetchPageContextResult>();
  fetch_result->annotated_page_content_result.emplace(AIPageContentResult());
  fetch_result->screenshot_result = base::unexpected(ScreenshotError{
      ScreenshotCaptureStatus::kCopyFromSurfaceError, "copy failed"});

  TabObservation observation;
  ASSERT_NO_FATAL_FAILURE(
      FetchObservationWithResult(std::move(fetch_result), observation));

  EXPECT_EQ(observation.result(), TabObservation::TAB_OBSERVATION_FETCH_ERROR);
  EXPECT_EQ(observation.screenshot_result(), TabObservation::SCREENSHOT_ERROR);

  RecordObservationHistogram(observation);
  histogram_tester.ExpectUniqueSample(
      kActorPageContextTabObservationResult,
      ActorTabObservationResult::kScreenshotError, 1);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       OnTabObservationFetchedMapsApcTimeout) {
  base::HistogramTester histogram_tester;
  auto fetch_result = std::make_unique<FetchPageContextResult>();
  fetch_result->annotated_page_content_result = base::unexpected(
      AIPageContentError{AIPageContentExtractionStatus::kMainFrameTimeout,
                         "Main frame timed out"});
  fetch_result->screenshot_result.emplace(MakeScreenshotResult());

  TabObservation observation;
  ASSERT_NO_FATAL_FAILURE(
      FetchObservationWithResult(std::move(fetch_result), observation));

  EXPECT_EQ(observation.result(), TabObservation::TAB_OBSERVATION_FETCH_ERROR);
  EXPECT_EQ(observation.annotated_page_content_result(),
            TabObservation::ANNOTATED_PAGE_CONTENT_TIMEOUT);
  EXPECT_FALSE(observation.has_annotated_page_content());
  EXPECT_EQ(observation.screenshot_result(), TabObservation::SCREENSHOT_OK);
  EXPECT_TRUE(observation.has_screenshot());

  histogram_tester.ExpectUniqueSample(
      kActorPageContextApcStatus,
      AIPageContentExtractionStatus::kMainFrameTimeout, 1);
  histogram_tester.ExpectUniqueSample(kActorPageContextScreenshotStatus,
                                      ScreenshotCaptureStatus::kSuccess, 1);

  RecordObservationHistogram(observation);
  histogram_tester.ExpectUniqueSample(kActorPageContextTabObservationResult,
                                      ActorTabObservationResult::kApcTimeout,
                                      1);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       OnTabObservationFetchedMapsApcError) {
  base::HistogramTester histogram_tester;
  auto fetch_result = std::make_unique<FetchPageContextResult>();
  fetch_result->annotated_page_content_result = base::unexpected(
      AIPageContentError{AIPageContentExtractionStatus::
                             kMainFrameEmptyOrDisconnectedRendererResponse,
                         "Renderer disconnected"});
  fetch_result->screenshot_result.emplace(MakeScreenshotResult());

  TabObservation observation;
  ASSERT_NO_FATAL_FAILURE(
      FetchObservationWithResult(std::move(fetch_result), observation));

  EXPECT_EQ(observation.result(), TabObservation::TAB_OBSERVATION_FETCH_ERROR);
  EXPECT_EQ(observation.annotated_page_content_result(),
            TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
  EXPECT_EQ(observation.screenshot_result(), TabObservation::SCREENSHOT_OK);

  RecordObservationHistogram(observation);
  histogram_tester.ExpectUniqueSample(kActorPageContextTabObservationResult,
                                      ActorTabObservationResult::kApcError, 1);
}

}  // namespace actor
