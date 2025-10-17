// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <optional>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

using ::base::test::TestFuture;

namespace actor {

namespace {

class ActorKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  ActorKeyedServiceBrowserTest() {
    // TODO(crbug.com/443783931): Add test coverage for
    // kGlicTabScreenshotPaintPreviewBackend.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorKeyedServiceBrowserTest(const ActorKeyedServiceBrowserTest&) = delete;
  ActorKeyedServiceBrowserTest& operator=(const ActorKeyedServiceBrowserTest&) =
      delete;

  ~ActorKeyedServiceBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    SetUpBlocklist(command_line, "blocked.example.com");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());

    // Optimization guide uses this histogram to signal initialization in tests.
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    // Simulate the component loading, as the implementation checks it, but the
    // actual list is set via the command line.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(
            {base::Version("123"),
             temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});
  }

 protected:
  tabs::TabInterface* active_tab() {
    return browser()->tab_strip_model()->GetActiveTab();
  }

  content::WebContents* web_contents() { return active_tab()->GetContents(); }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->profile());
  }

 private:
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest, StartStopTask) {
  TaskId first_task_id = actor_keyed_service()->CreateTask();
  EXPECT_FALSE(first_task_id.is_null());

  actor_keyed_service()->StopTask(first_task_id, /*success=*/true);

  TaskId second_task_id = actor_keyed_service()->CreateTask();
  EXPECT_FALSE(first_task_id.is_null());
  EXPECT_NE(first_task_id, second_task_id);
}

// TODO(crbug.com/439247740): Fails on Win ASan.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_StartNavigateStopTask DISABLED_StartNavigateStopTask
#else
#define MAYBE_StartNavigateStopTask StartNavigateStopTask
#endif
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       MAYBE_StartNavigateStopTask) {
  TaskId first_task_id = actor_keyed_service()->CreateTask();
  EXPECT_FALSE(first_task_id.is_null());

  PerformActionsFuture result_future;
  const GURL url = embedded_https_test_server().GetURL("/actor/blank.html");
  std::unique_ptr<ToolRequest> action_request =
      std::make_unique<NavigateToolRequest>(
          browser()->GetActiveTabInterface()->GetHandle(), url);
  actor_keyed_service()->PerformActions(
      first_task_id, ToRequestList(action_request), ActorTaskMetadata(),
      result_future.GetCallback());
  ExpectOkResult(result_future);
  EXPECT_FALSE(result_future.Get<1>().has_value());
  EXPECT_EQ(result_future.Get<2>().size(), 1u);
  EXPECT_EQ(web_contents()->GetURL(), url);

  actor_keyed_service()->StopTask(first_task_id, /*success=*/true);

  TaskId second_task_id = actor_keyed_service()->CreateTask();
  EXPECT_FALSE(first_task_id.is_null());
  EXPECT_NE(first_task_id, second_task_id);
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       RequestTabObservation_HasMetadata) {
  const GURL url(
      "data:text/html,<html><head>"
      "<meta name=\"sis\" content=\"rose\">"
      "<meta name=\"sis\" content=\"ruth\">"
      "<meta name=\"sis\" content=\"val\">"
      "</head><body>Hello</body></html>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  TaskId task_id = actor_keyed_service()->CreateTask();

  TestFuture<ActorKeyedService::TabObservationResult> future;
  actor_keyed_service()->RequestTabObservation(*active_tab(), task_id,
                                               future.GetCallback());

  const ActorKeyedService::TabObservationResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());

  optimization_guide::proto::TabObservation observation;
  FillInTabObservation(**result, observation);

  ASSERT_TRUE(observation.has_metadata());
  const auto& metadata = observation.metadata();
  ASSERT_EQ(metadata.frame_metadata_size(), 1);
  const auto& frame_metadata = metadata.frame_metadata(0);
  ASSERT_EQ(frame_metadata.meta_tags_size(), 3);
  EXPECT_EQ(frame_metadata.meta_tags(0).name(), "sis");
  EXPECT_EQ(frame_metadata.meta_tags(0).content(), "rose");
  EXPECT_EQ(frame_metadata.meta_tags(1).name(), "sis");
  EXPECT_EQ(frame_metadata.meta_tags(1).content(), "ruth");
  EXPECT_EQ(frame_metadata.meta_tags(2).name(), "sis");
  EXPECT_EQ(frame_metadata.meta_tags(2).content(), "val");

  actor_keyed_service()->StopTask(task_id, /*success=*/true);
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       RequestTabObservationSkipCrashedMainFrame) {
  TaskId task_id = actor_keyed_service()->CreateTask();

  // Crash the main frame.
  {
    auto* main_frame_proc = web_contents()->GetPrimaryMainFrame()->GetProcess();
    content::RenderProcessHostWatcher crashed_obs(
        main_frame_proc,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    main_frame_proc->Shutdown(content::RESULT_CODE_KILLED);
    crashed_obs.Wait();
  }

  TestFuture<ActorKeyedService::TabObservationResult> future;
  actor_keyed_service()->RequestTabObservation(*active_tab(), task_id,
                                               future.GetCallback());

  const ActorKeyedService::TabObservationResult& result = future.Get();
  ASSERT_FALSE(result.has_value());
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       RequestTabObservationSkipAsyncObservationInformation) {
  TaskId task_id = actor_keyed_service()->CreateTask();
  // Navigate the active tab to a new page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL("/actor/blank.html")));

  actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
  TestFuture<mojom::ActionResultPtr> add_tab_future;
  task->AddTab(browser()->GetActiveTabInterface()->GetHandle(),
               add_tab_future.GetCallback());
  auto add_tab_result = add_tab_future.Take();
  ASSERT_TRUE(add_tab_result);

  TestFuture<std::unique_ptr<optimization_guide::proto::ActionsResult>,
             std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>>
      future;
  actor::BuildActionsResultWithObservations(
      *browser()->profile(), base::TimeTicks::Now(),
      mojom::ActionResultCode::kOk, std::nullopt,
      std::vector<actor::ActionResultWithLatencyInfo>(), *task, true,
      future.GetCallback());
  auto [actions_result, _] = future.Take();
  ASSERT_TRUE(actions_result);
  EXPECT_EQ(actions_result->action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kOk));
  EXPECT_EQ(actions_result->tabs_size(), 1);
  EXPECT_FALSE(actions_result->tabs()[0].has_annotated_page_content());
  EXPECT_FALSE(actions_result->tabs()[0].has_screenshot());
}

class ActorKeyedServiceOriginGatingBrowserTest
    : public ActorKeyedServiceBrowserTest {
 public:
  ActorKeyedServiceOriginGatingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor,
                              kGlicCrossOriginNavigationGating},
        /*disabled_features=*/{features::kGlicWarming});
  }

  void CreateMockPromptIPCResponse(
      std::optional<url::Origin> expected_navigation_origin,
      bool permission_granted) {
    user_confirmation_dialog_subscription_ =
        actor_keyed_service()
            ->AddRequestToShowUserConfirmationDialogSubscriberCallback(
                base::BindLambdaForTesting(
                    [expected_navigation_origin, permission_granted](
                        const std::optional<url::Origin>& got_navigation_origin,
                        const std::optional<int32_t> got_download_id,
                        ActorKeyedService::UserConfirmationDialogCallback
                            callback) {
                      EXPECT_EQ(got_navigation_origin,
                                expected_navigation_origin);
                      EXPECT_FALSE(got_download_id);
                      // Send a mock IPC response.
                      std::move(callback).Run(
                          webui::mojom::UserConfirmationDialogResponse::New(
                              webui::mojom::UserConfirmationDialogResult::
                                  NewPermissionGranted(permission_granted)));
                    }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription user_confirmation_dialog_subscription_;
};

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceOriginGatingBrowserTest,
                       AddWritableMainframeOrigins) {
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com", base::StrCat({"/actor/link_full_page.html?href=",
                               EncodeURI(cross_origin_url.spec())}));

  // Navigate the active tab to the link page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), link_page_url));

  CreateMockPromptIPCResponse(url::Origin::Create(cross_origin_url),
                              /*permission_granted=*/false);

  TaskId task_id = actor_keyed_service()->CreateTask();

  PerformActionsFuture result1;
  actor_keyed_service()->PerformActions(
      task_id, ToRequestList(MakeClickRequest(*active_tab(), gfx::Point(1, 1))),
      ActorTaskMetadata(), result1.GetCallback());
  ExpectErrorResult(result1,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);

  PerformActionsFuture result2;
  actor_keyed_service()->PerformActions(
      task_id, ToRequestList(MakeClickRequest(*active_tab(), gfx::Point(1, 1))),
      ActorTaskMetadata::WithAddedWritableMainframeOriginsForTesting(
          {url::Origin::Create(cross_origin_url)}),
      result2.GetCallback());
  ExpectOkResult(result2);
}

}  // namespace
}  // namespace actor
