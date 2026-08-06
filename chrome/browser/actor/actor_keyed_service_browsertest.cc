// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <optional>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_browsertest.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/ui_test_utils.h"
#endif
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#else
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

using ::base::test::TestFuture;

namespace actor {

ActorKeyedServiceBrowserTest::ActorKeyedServiceBrowserTest() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {features::kGlic, {}},
          {features::kGlicActor, {}},
          {blink::features::kAIPageContentTrackedElementsIframe, {}},
          {page_content_annotations::kGlicTabScreenshotExperiment,
           {{"screenshot_timeout_ms", "30s"}}},
      },
      /*disabled_features=*/{features::kGlicWarming});
}

ActorKeyedServiceBrowserTest::~ActorKeyedServiceBrowserTest() = default;

void ActorKeyedServiceBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);
  SetUpBlocklist(command_line, "blocked.example.com");
#if BUILDFLAG(IS_CHROMEOS)
  command_line->AppendSwitch(ash::switches::kIgnoreUserProfileMappingForTests);
#endif
}

void ActorKeyedServiceBrowserTest::SetUpOnMainThread() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/517619366): Decouple test from Glic eligibility criteria.
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_S) {
    GTEST_SKIP() << "Actor requires Android S+ to run";
  }
#endif
  PlatformBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->ServeFilesFromSourceDirectory("components/test/data");
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      "components/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Optimization guide uses this histogram to signal initialization in tests.
  auto* optimization_guide_init_histogram =
      "OptimizationGuide.HintsManager.HintCacheInitialized";
  if (histogram_tester_for_init_.GetTotalSum(
          optimization_guide_init_histogram) == 0) {
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_, optimization_guide_init_histogram, 1);
  }

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->MaybeUpdateHintsComponent(
          {base::Version("123"),
           temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});
}

tabs::TabInterface* ActorKeyedServiceBrowserTest::active_tab() {
  return chrome_test_utils::GetActiveTab(this);
}

content::WebContents* ActorKeyedServiceBrowserTest::web_contents() {
  return active_tab()->GetContents();
}

content::RenderFrameHost* ActorKeyedServiceBrowserTest::main_frame() {
  return web_contents()->GetPrimaryMainFrame();
}

ActorKeyedService* ActorKeyedServiceBrowserTest::actor_keyed_service() {
  return ActorKeyedService::Get(GetProfile());
}

namespace {

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest, StartStopTask) {
  TaskId first_task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  EXPECT_FALSE(first_task_id.is_null());

  actor_keyed_service()->StopTask(first_task_id,
                                  ActorTask::StoppedReason::kTaskComplete);

  TaskId second_task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  EXPECT_FALSE(first_task_id.is_null());
  EXPECT_NE(first_task_id, second_task_id);
}

// TODO(crbug.com/439247740): Fails on Win ASan and Android.
#if (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)) || BUILDFLAG(IS_ANDROID)
#define MAYBE_StartNavigateStopTask DISABLED_StartNavigateStopTask
#else
#define MAYBE_StartNavigateStopTask StartNavigateStopTask
#endif
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       MAYBE_StartNavigateStopTask) {
  TaskId first_task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  EXPECT_FALSE(first_task_id.is_null());

  PerformActionsFuture result_future;
  const GURL url = embedded_https_test_server().GetURL("/actor/blank.html");
  std::unique_ptr<ToolRequest> action_request =
      std::make_unique<NavigateToolRequest>(active_tab()->GetHandle(), url);
  actor_keyed_service()->PerformActions(
      first_task_id, ToRequestList(action_request), ActorTaskMetadata(),
      result_future.GetCallback());
  ExpectOkResult(result_future);
  EXPECT_EQ(result_future.Get().size(), 1u);
  EXPECT_EQ(web_contents()->GetURL(), url);

  actor_keyed_service()->StopTask(first_task_id,
                                  ActorTask::StoppedReason::kTaskComplete);

  TaskId second_task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
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
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), url));

  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  TestFuture<ActorKeyedService::TabObservationResult> future;
  actor_keyed_service()->RequestTabObservation(
      *active_tab(), task_id, std::nullopt, future.GetCallback());

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

  actor_keyed_service()->StopTask(task_id,
                                  ActorTask::StoppedReason::kTaskComplete);
}

// TODO(b/484011242): Fix flakiness and re-enable this test on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_RequestTabObservation_HasScreenshotInfo \
  DISABLED_RequestTabObservation_HasScreenshotInfo
#else
#define MAYBE_RequestTabObservation_HasScreenshotInfo \
  RequestTabObservation_HasScreenshotInfo
#endif
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       MAYBE_RequestTabObservation_HasScreenshotInfo) {
  const GURL url =
      embedded_https_test_server().GetURL("/actor/simple_iframe.html");
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();

  // Wait for main frame layout/render.
  {
    TestFuture<bool> future;
    main_frame->GetRenderWidgetHost()->InsertVisualStateCallback(
        future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
  }

  // Wait for child frame layout/render.
  {
    content::RenderFrameHost* child_frame =
        content::ChildFrameAt(main_frame, 0);
    ASSERT_TRUE(child_frame);

    TestFuture<bool> future;
    child_frame->GetRenderWidgetHost()->InsertVisualStateCallback(
        future.GetCallback());
    ASSERT_TRUE(future.Wait())
        << "Timeout waiting for syncing with subframe renderer";
  }

  content::WaitForCopyableViewInWebContents(web_contents());

  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  TestFuture<ActorKeyedService::TabObservationResult> future;
  actor_keyed_service()->RequestTabObservation(
      *active_tab(), task_id, std::nullopt, future.GetCallback());

  const ActorKeyedService::TabObservationResult& result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());

  optimization_guide::proto::TabObservation observation;
  FillInTabObservation(**result, observation);

  ASSERT_TRUE(observation.has_screenshot_info());
  const auto& screenshot_info = observation.screenshot_info();
  ASSERT_EQ(screenshot_info.iframe_info_size(), 1);
  const auto& iframe_info = screenshot_info.iframe_info(0);
  EXPECT_EQ(iframe_info.url(),
            embedded_https_test_server().GetURL("/actor/blank.html").spec());
  EXPECT_TRUE(iframe_info.has_bounding_box());
  EXPECT_GE(iframe_info.bounding_box().x(), 0);
  EXPECT_GE(iframe_info.bounding_box().y(), 0);
  EXPECT_GT(iframe_info.bounding_box().width(), 0);
  EXPECT_GT(iframe_info.bounding_box().height(), 0);

  actor_keyed_service()->StopTask(task_id,
                                  ActorTask::StoppedReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       RequestTabObservationSkipCrashedMainFrame) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

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
  actor_keyed_service()->RequestTabObservation(
      *active_tab(), task_id, std::nullopt, future.GetCallback());

  const ActorKeyedService::TabObservationResult& result = future.Get();
  std::optional<std::string> error_message =
      ActorKeyedService::ExtractErrorMessageIfFailed(result);
  ASSERT_TRUE(result.has_value());
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       CreateActorTabInBackground) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("/actor/blank.html")));

  actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
  actor::AddTabToTask(*active_tab(), *task);

  TestFuture<tabs::TabInterface*> future;
  SessionID initiator_window_id =
      sessions::SessionTabHelper::IdForTab(web_contents());

  // Call CreateActorTab to open a background tab.
  actor_keyed_service()->CreateActorTab(
      task_id, /* open_in_background = */ true, active_tab()->GetHandle(),
      initiator_window_id, future.GetCallback());

  tabs::TabInterface* new_tab = future.Get();

  // Make sure we can run actions on this new tab (this also ensures all new tab
  // animations are completed).
  PerformActionsFuture result_future;
  const GURL url = embedded_https_test_server().GetURL("/actor/simple.html");
  std::unique_ptr<ToolRequest> action_request =
      std::make_unique<NavigateToolRequest>(new_tab->GetHandle(), url);
  actor_keyed_service()->PerformActions(task_id, ToRequestList(action_request),
                                        ActorTaskMetadata(),
                                        result_future.GetCallback());
  ExpectOkResult(result_future);

  // New tab should remain in the background.
  ASSERT_NE(active_tab()->GetHandle(), new_tab->GetHandle());
  ASSERT_FALSE(new_tab->IsActivated());
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       RequestTabObservationSkipAsyncObservationInformation) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  // Navigate the active tab to a new page.
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("/actor/blank.html")));

  actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
  actor::AddTabToTask(*active_tab(), *task);

  TestFuture<base::TimeTicks /*start_time*/,
             std::vector<actor::ActionResultWithLatencyInfo>, actor::TaskId,
             bool /*skip_async_observation_information*/,
             std::optional<page_content_annotations::ScreenshotOptions::
                               ScreenshotCollectionOptions>,
             std::unique_ptr<optimization_guide::proto::ActionsResult>,
             std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>>
      future;
  actor::BuildActionsResultWithObservations(
      *GetProfile(), base::TimeTicks::Now(),
      std::vector<actor::ActionResultWithLatencyInfo>(), *task, true,
      std::nullopt, future.GetCallback());
  const std::unique_ptr<optimization_guide::proto::ActionsResult>&
      actions_result = future.Get<5>();
  ASSERT_TRUE(actions_result);
  EXPECT_EQ(actions_result->action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kOk));
  EXPECT_EQ(actions_result->tabs_size(), 1);
  EXPECT_FALSE(actions_result->tabs()[0].has_annotated_page_content());
  EXPECT_FALSE(actions_result->tabs()[0].has_screenshot());
}

// Verify that optimization_guide::proto::ActionsResult::extra_information is
// populated with extra information from the action results.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       BuildActionsResultWithExtraInformation) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);

  std::vector<actor::ActionResultWithLatencyInfo> action_results;

  // Add a successful result with an extra_information message
  // "Some extra info".
  auto ok_result = MakeOkResultWithMessage(/*requires_page_stabilization=*/true,
                                           "Some extra info");
  action_results.emplace_back(base::TimeTicks::Now(), base::TimeTicks::Now(),
                              std::move(ok_result));

  // Add a failed result.
  auto fail_result = mojom::ActionResult::New(
      mojom::ActionResultCode::kFormFillingDialogError,
      /*requires_page_stabilization=*/false, "Error message",
      /*script_tool_response=*/nullptr,
      /*execution_end_time=*/base::TimeTicks::Now(),
      mojom::ScreenshotPolicy::kRequested,
      mojom::PageContentExtractionPolicy::kRequested,
      /*attempt_login_status=*/std::nullopt);
  action_results.emplace_back(base::TimeTicks::Now(), base::TimeTicks::Now(),
                              std::move(fail_result));

  TestFuture<base::TimeTicks, std::vector<actor::ActionResultWithLatencyInfo>,
             actor::TaskId, bool,
             std::optional<page_content_annotations::ScreenshotOptions::
                               ScreenshotCollectionOptions>,
             std::unique_ptr<optimization_guide::proto::ActionsResult>,
             std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>>
      future;

  actor::BuildActionsResultWithObservations(
      *GetProfile(), base::TimeTicks::Now(), std::move(action_results), *task,
      /*skip_async_observation_information=*/true, std::nullopt,
      future.GetCallback());

  const std::unique_ptr<optimization_guide::proto::ActionsResult>&
      actions_result = future.Get<5>();
  ASSERT_TRUE(actions_result);

  // We expect 2 entries in extra_information.
  // The first one should be "Some extra info".
  // The second one should be empty because it failed.
  ASSERT_EQ(actions_result->extra_information_size(), 2);
  EXPECT_EQ(actions_result->extra_information(0), "Some extra info");
  EXPECT_EQ(actions_result->extra_information(1), "");

  // The error message should be "Error message".
  EXPECT_EQ(actions_result->error_message(), "Error message");
  EXPECT_EQ(
      actions_result->action_result(),
      static_cast<int32_t>(mojom::ActionResultCode::kFormFillingDialogError));
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       AddsTabBlockedByCrossProfileCheck) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path);

  BrowserWindowInterface* browser2 = CreateBrowserWindow(
      BrowserWindowCreateParams(&profile2, /*from_user_gesture=*/true));
  chrome::NewTab(browser2, NewTabTypes::kNoUserAction);
  tabs::TabInterface* tab2 = browser2->GetActiveTabInterface();

  ActorTask* task = actor_keyed_service()->GetTask(task_id);

  base::test::TestFuture<mojom::ActionResultPtr> future;
  task->AddTab(tab2->GetHandle(), /*stop_task_on_detach=*/true,
               future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kTaskWentAway);

  browser2->GetWindow()->Close();
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       ObserveTabBlockedByCrossProfileCheck) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path);

  BrowserWindowInterface* browser2 = CreateBrowserWindow(
      BrowserWindowCreateParams(&profile2, /*from_user_gesture=*/true));
  chrome::NewTab(browser2, NewTabTypes::kNoUserAction);
  tabs::TabInterface* tab2 = browser2->GetActiveTabInterface();

  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  task->SetState(ActorTask::State::kActing);

  task->ObserveTabOnce(tab2->GetHandle());

  EXPECT_FALSE(task->GetLastActedTabs().contains(tab2->GetHandle()));

  browser2->GetWindow()->Close();
}
#endif
}  // namespace
}  // namespace actor
