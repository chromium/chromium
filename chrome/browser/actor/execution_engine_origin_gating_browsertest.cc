// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/safety_list_manager.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/core/filters/hints_component_util.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace actor {

namespace {

constexpr char kHandleUserConfirmationDialogTempl[] =
    R"js(
  (() => {
    window.userConfirmationDialogRequestData = new Promise(resolve => {
      const subscription = client.browser
          .selectUserConfirmationDialogRequestHandler()
          .subscribe(
        request => {
          // Response will be verified in C++ callback below.
          request.onDialogClosed({
            response: {
              permissionGranted: $1,
            },
          });
          // Resolve the promise with the request data to be verified.
          resolve(request);
          subscription.unsubscribe();
        }
      );
    });
  })();
)js";

constexpr char kHandleNavigationConfirmationTempl[] =
    R"js(
  (() => {
    window.navigationConfirmationRequestData = new Promise(resolve => {
      const subscription = client.browser
          .selectNavigationConfirmationRequestHandler()
          .subscribe(
            request => {
              // Response will be verified in C++ callback below.
              request.onConfirmationDecision({
                response: {
                  permissionGranted: $1,
                },
              });
              // Resolve the promise with the request data to be verified.
              resolve(request);
            }
          );
    });
  })();
)js";

constexpr char kSetUpDelayedNavigationConfirmationRequestHandler[] =
    R"js(
  (() => {
    client.browser
      .selectNavigationConfirmationRequestHandler()
      .subscribe(
        request => {
          window.pendingRequest = request;
          // Respond to any pending checks
          if (window.signalRequestIsPending) {
            const temp = window.signalRequestIsPending;
            window.signalRequestIsPending = null;
            temp(request);
          }
        }
      );
  })();
)js";

constexpr char kSetUpDelayedUserConfirmationDialogRequestHandler[] =
    R"js(
  (() => {
    client.browser
      .selectUserConfirmationDialogRequestHandler()
      .subscribe(
        request => {
          window.pendingRequest = request;
          // Respond to any pending checks
          if (window.signalRequestIsPending) {
            const temp = window.signalRequestIsPending;
            window.signalRequestIsPending = null;
            temp(request);
          }
        }
      );
  })();
)js";

constexpr std::string_view kSameOriginSourceHistogram =
    "Actor.NavigationGating.SameOriginSource";
constexpr std::string_view kSameSiteSourceHistogram =
    "Actor.NavigationGating.SameSiteSource";
constexpr std::string_view kSameOriginInitiatorHistogram =
    "Actor.NavigationGating.SameOriginInitiator";
constexpr std::string_view kSameSiteInitiatorHistogram =
    "Actor.NavigationGating.SameSiteInitiator";

}  // namespace

// TODO(crbug.com/537849016): Simplify this test suite to GlicBrowserTest.
class ExecutionEngineOriginGatingBrowserTestBase
    : public glic::NonInteractiveGlicTest {
 public:
  ExecutionEngineOriginGatingBrowserTestBase()
      : ExecutionEngineOriginGatingBrowserTestBase(
            /*additional_enabled_features=*/{},
            /*additional_disabled_features=*/{}) {}

  ExecutionEngineOriginGatingBrowserTestBase(
      const std::vector<base::test::FeatureRefAndParams>&
          additional_enabled_features,
      const std::vector<base::test::FeatureRef>& additional_disabled_features) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlic, {}},
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}},
        {kGlicCrossOriginNavigationGating,
         {
             {"confirm_navigation_to_new_origins", "true"},
         }},
    };
    for (const auto& feat : additional_enabled_features) {
      enabled_features.push_back(feat);
    }
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kGlicWarming};
    for (const auto& feat : additional_disabled_features) {
      disabled_features.push_back(feat);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  ~ExecutionEngineOriginGatingBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data");
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    // Optimization guide uses this histogram to signal initialization in tests.
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath proto_path =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("base_proto.pb"));
    ASSERT_TRUE(SetUpOptimizationGuideComponentBlocklist(
        proto_path, "sensitive.example.com"));
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent({base::Version("1"), proto_path});

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  [[nodiscard]] InteractiveTestApi::MultiStep CreateMockWebClientRequest(
      const std::string_view handle_dialog_js,
      const base::Location& location = FROM_HERE) {
    return InAnyContext(WithElement(
        glic::kGlicContentsElementId,
        [handle_dialog_js, location](::ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          ASSERT_TRUE(content::ExecJs(glic_contents, handle_dialog_js))
              << ", expected at " << location.ToString();
        }));
  }

  [[nodiscard]] InteractiveTestApi::MultiStep
  VerifyUserConfirmationDialogRequest(
      const base::DictValue& expected_request,
      const base::Location& location = FROM_HERE) {
    static constexpr char kGetUserConfirmationDialogRequest[] =
        R"js(
          (() => {
            return window.userConfirmationDialogRequestData;
          })();
        )js";
    return VerifyWebClientRequest(kGetUserConfirmationDialogRequest,
                                  expected_request, location);
  }

  [[nodiscard]] InteractiveTestApi::MultiStep
  VerifyNavigationConfirmationRequest(
      const base::DictValue& expected_request,
      const base::Location& location = FROM_HERE) {
    static constexpr char kGetNavigationConfirmationRequestData[] =
        R"js(
          (() => {
            return window.navigationConfirmationRequestData;
          })();
        )js";
    return VerifyWebClientRequest(kGetNavigationConfirmationRequestData,
                                  expected_request, location);
  }

  [[nodiscard]] InteractiveTestApi::MultiStep
  WaitUntilPendingConfirmationRequest(
      const base::DictValue& expected_request,
      const base::Location& location = FROM_HERE) {
    static constexpr char kGetNavigationConfirmationRequestData[] =
        R"js(
  (() => {
    if (window.pendingRequest) {
      return window.pendingRequest;
    }
    // Request might have not came yet. Make pending check.
    return new Promise(resolve => {
      window.signalRequestIsPending = resolve;
    });
  })();
)js";
    return VerifyWebClientRequest(kGetNavigationConfirmationRequestData,
                                  expected_request, location);
  }

  [[nodiscard]] InteractiveTestApi::MultiStep RespondToPendingRequest(
      bool permission_granted,
      const base::Location& location = FROM_HERE) {
    return InAnyContext(WithElement(
        glic::kGlicContentsElementId,
        [permission_granted, location](::ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          ASSERT_TRUE(content::ExecJs(glic_contents, content::JsReplace(
                                                         R"js(
                    (async () => {
                      let request = window.pendingRequest;
                      if (!request) {
                        const {promise, resolve} = Promise.withResolvers();
                        window.signalRequestIsPending = resolve;
                        request = await promise;
                      }
                      const arg = {
                        response: {
                          permissionGranted: $1,
                        },
                      };
                      if (request.onConfirmationDecision) {
                        request.onConfirmationDecision(arg);
                      } else if (request.onDialogClosed) {
                        request.onDialogClosed(arg);
                      }
                      window.pendingRequest = null;
                    })();
                  )js",
                                                         permission_granted)))
              << ", expected at " << location.ToString();
        }));
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }
  ActorKeyedService& actor_keyed_service() {
    return *ActorKeyedService::Get(browser()->GetProfile());
  }
  ActorTask& actor_task() { return *actor_keyed_service().GetTask(task_id_); }
  tabs::TabInterface* active_tab() {
    return browser()->GetActiveTabInterface();
  }

  void StopAllTasks() {
    actor_keyed_service().ResetForTesting();
    // Tasks are deleted asynchronously; return only when the task is deleted.
    WaitForPostedTask();
  }

  void ClickTarget(
      std::string_view query_selector,
      mojom::ActionResultCode expected_code = mojom::ActionResultCode::kOk) {
    std::optional<int> dom_node_id =
        content::GetDOMNodeId(*main_frame(), query_selector);
    ASSERT_TRUE(dom_node_id);
    std::unique_ptr<ToolRequest> click =
        MakeClickRequest(*main_frame(), dom_node_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(click), result.GetCallback());
    if (expected_code == mojom::ActionResultCode::kOk) {
      ExpectOkResult(result);
    } else {
      ExpectErrorResult(result, expected_code);
    }
  }

  InteractiveTestApi::MultiStep VerifyWebClientRequest(
      const std::string_view get_request_js,
      const base::DictValue& expected_request,
      const base::Location& location) {
    return InAnyContext(WithElement(
        glic::kGlicContentsElementId,
        [&, get_request_js](::ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          auto eval_result = content::EvalJs(glic_contents, get_request_js);
          const auto& actual_request = eval_result.ExtractDict();
          ASSERT_EQ(expected_request, actual_request)
              << ", expected at " << location.ToString();
        }));
  }

  void OpenGlicAndCreateTask() {
    RunTestSequence(OpenGlic());
    TrackGlicInstanceWithTabIndex(
        InProcessBrowserTest::browser()->GetTabStripModel()->active_index());
    CreateTaskForActiveTab();
  }

  void CreateTaskForActiveTab() {
    base::test::TestFuture<
        base::expected<int32_t, glic::mojom::CreateTaskErrorReason>>
        create_task_future;
    ASSERT_TRUE(GetGlicInstanceImpl());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetGlicInstanceImpl()
                 ->GetActorTaskManager()
                 ->GetClientSessionForTesting() != nullptr;
    }));
    GetGlicInstanceImpl()
        ->GetActorTaskManager()
        ->GetClientSessionForTesting()
        ->CreateTask(actor::webui::mojom::TaskOptions::New(),
                     create_task_future.GetCallback());
    auto result = create_task_future.Get();
    ASSERT_TRUE(result.has_value());
    task_id_ = TaskId(result.value());
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TaskId task_id_;
};

class ExecutionEngineOriginGatingBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase {
 public:
  ExecutionEngineOriginGatingBrowserTest()
      : ExecutionEngineOriginGatingBrowserTest(
            /*additional_enabled_features=*/{},
            /*additional_disabled_features=*/{}) {}

  ExecutionEngineOriginGatingBrowserTest(
      const std::vector<base::test::FeatureRefAndParams>&
          additional_enabled_features,
      const std::vector<base::test::FeatureRef>& additional_disabled_features)
      : ExecutionEngineOriginGatingBrowserTestBase(
            additional_enabled_features,
            additional_disabled_features) {}
  ~ExecutionEngineOriginGatingBrowserTest() override = default;

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmNavigationToNewOrigin_Granted) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);
  RunTestSequence(VerifyNavigationConfirmationRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "taskId": $2})",
          url::Origin::Create(second_url), actor_task().id().value()))));

  // The first navigation should log that gating was not applied. The second
  // should log that gating was applied.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Actor.NavigationGating.AppliedGate"),
      base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(kSameOriginSourceHistogram),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kSameSiteSourceHistogram),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kSameOriginInitiatorHistogram),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kSameSiteInitiatorHistogram),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));

  // Should log that permission was *granted* once.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);

  const auto ukm_entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::Actor_OriginGating::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  // Same origin navigation should not have triggered UKM
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[0], "ServerConfirmationResult", /*expected_value=*/
      static_cast<int64_t>(
          ExecutionEngine::ActorServerConfirmationResult::kAccepted));
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[0], "EngineState", /*expected_value=*/
      static_cast<int64_t>(ExecutionEngine::State::kToolInvoke));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmNavigationToNewOrigin_Denied) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  RunTestSequence(VerifyNavigationConfirmationRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "taskId": $2})",
          url::Origin::Create(second_url), actor_task().id().value()))));

  // Should log that permission was *denied* once.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", false, 1);

  const auto ukm_entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::Actor_OriginGating::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  // Same origin navigation should not have triggered UKM
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[0], "ServerConfirmationResult", /*expected_value=*/
      static_cast<int64_t>(
          ExecutionEngine::ActorServerConfirmationResult::kRejected));
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[0], "EngineState", /*expected_value=*/
      static_cast<int64_t>(ExecutionEngine::State::kToolInvoke));
}

class ExecutionEngineOriginGatingBFCacheBrowserTest
    : public ExecutionEngineOriginGatingBrowserTest {
 public:
  ExecutionEngineOriginGatingBFCacheBrowserTest()
      : ExecutionEngineOriginGatingBrowserTest(
            content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
            content::GetDefaultDisabledBackForwardCacheFeaturesForTesting()) {}
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBFCacheBrowserTest,
                       ConfirmNavigationToNewOrigin_BFCacheRestore_Denied) {
  base::HistogramTester histogram_tester;
  const GURL first_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  RunTestSequence(OpenGlic());
  TrackGlicInstanceWithTabIndex(browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));
  content::RenderFrameHostWrapper rfh(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));
  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  CreateTaskForActiveTab();

  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Deny restoring foo.com.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('link').removeAttribute('href');"
      "document.getElementById('link').onclick = () => { history.back(); };"));

  content::TestNavigationObserver observer(web_contents());
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  observer.Wait();

  EXPECT_EQ(web_contents()->GetURL(), second_url);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBFCacheBrowserTest,
                       ConfirmNavigationToNewOrigin_BFCacheRestore_Granted) {
  base::HistogramTester histogram_tester;
  const GURL first_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  RunTestSequence(OpenGlic());
  TrackGlicInstanceWithTabIndex(browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));
  content::RenderFrameHostWrapper rfh(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));
  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  CreateTaskForActiveTab();

  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Allow restoring foo.com.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('link').removeAttribute('href');"
      "document.getElementById('link').onclick = () => { history.back(); };"));

  content::TestNavigationObserver observer(web_contents());
  ClickTarget("#link", mojom::ActionResultCode::kOk);
  observer.Wait();

  EXPECT_EQ(web_contents()->GetURL(), first_url);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

IN_PROC_BROWSER_TEST_F(
    ExecutionEngineOriginGatingBFCacheBrowserTest,
    PausedTask_ConfirmNavigationToNewOrigin_BFCacheRestore_Denied) {
  base::HistogramTester histogram_tester;
  const GURL first_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  RunTestSequence(OpenGlic());
  TrackGlicInstanceWithTabIndex(browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));
  content::RenderFrameHostWrapper rfh(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));
  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  CreateTaskForActiveTab();
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());
  ASSERT_TRUE(actor_task().HasTab(active_tab()->GetHandle()));

  // Pause the task so that it is under user control.
  actor_task().Pause(/*from_actor=*/true);
  ASSERT_TRUE(actor_task().IsUnderUserControl());
  ASSERT_FALSE(actor_task().IsActingOnTab(active_tab()->GetHandle()));
  ASSERT_TRUE(actor_task().HasTab(active_tab()->GetHandle()));

  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Deny restoring foo.com.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  content::TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(content::ExecJs(web_contents(), "history.back();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();

  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), second_url);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_F(
    ExecutionEngineOriginGatingBFCacheBrowserTest,
    PausedTask_ConfirmNavigationToNewOrigin_BFCacheRestore_Granted) {
  base::HistogramTester histogram_tester;
  const GURL first_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  RunTestSequence(OpenGlic());
  TrackGlicInstanceWithTabIndex(browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));
  content::RenderFrameHostWrapper rfh(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));
  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  CreateTaskForActiveTab();
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());
  ASSERT_TRUE(actor_task().HasTab(active_tab()->GetHandle()));

  // Pause the task so that it is under user control.
  actor_task().Pause(/*from_actor=*/true);
  ASSERT_TRUE(actor_task().IsUnderUserControl());
  ASSERT_FALSE(actor_task().IsActingOnTab(active_tab()->GetHandle()));
  ASSERT_TRUE(actor_task().HasTab(active_tab()->GetHandle()));

  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Allow restoring foo.com.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  content::TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(content::ExecJs(web_contents(), "history.back();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();

  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), first_url);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

class ExecutionEngineOriginGatingPrerenderBrowserTest
    : public ExecutionEngineOriginGatingBrowserTest {
 public:
  ExecutionEngineOriginGatingPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ExecutionEngineOriginGatingPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingPrerenderBrowserTest,
                       ConfirmNavigationToNewOrigin_Prerender_Denied) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url = embedded_https_test_server().GetURL(
      "sub.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  // Prerender second_url before the task starts.
  prerender_helper_.AddPrerender(second_url);

  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  // Navigation to prerendered page should be blocked by origin gating.
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingPrerenderBrowserTest,
                       ConfirmNavigationToNewOrigin_Prerender_Granted) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url = embedded_https_test_server().GetURL(
      "sub.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  // Prerender second_url before the task starts.
  prerender_helper_.AddPrerender(second_url);

  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  // Navigation to prerendered page should be allowed.
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

class ExecutionEngineOriginGatingExplicitGrantBrowserTest
    : public ExecutionEngineOriginGatingBrowserTest {
 public:
  ExecutionEngineOriginGatingExplicitGrantBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicCrossOriginNavigationGating,
        {{"allow_implicit_tool_origin_grants", "false"},
         {"confirm_navigation_to_new_origins", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingExplicitGrantBrowserTest,
                       ImplicitGrantDisabled) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  const GURL destination_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  std::unique_ptr<ToolRequest> navigate =
      MakeNavigateRequest(*active_tab(), destination_url.spec());
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate), result.GetCallback());

  // Since implicit grant is disabled, it should try to prompt and get denied.
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmSensitiveOriginWithUser_Granted) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sensitive_url = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", sensitive_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(sensitive_url)))));

  // The first navigation should log that gating was not applied. The second
  // should log that gating was applied.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Actor.NavigationGating.AppliedGate"),
      base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(kSameOriginSourceHistogram),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  histogram_tester.ExpectUniqueSample(kSameSiteSourceHistogram, true, 2);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSameOriginInitiatorHistogram),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  histogram_tester.ExpectUniqueSample(kSameSiteInitiatorHistogram, true, 2);
  // Should log that permission was *granted* once.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

class ExecutionEngineOriginGatingUserPromptingBrowserTest
    : public ExecutionEngineOriginGatingBrowserTest {
 public:
  ExecutionEngineOriginGatingUserPromptingBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic, {}},
            {features::kGlicActor,
             {{features::kGlicActorPolicyControlExemption.name, "true"}}},
            {kGlicCrossOriginNavigationGating,
             {{
                 {"confirm_navigation_to_new_origins", "true"},
                 {std::string(kGlicPromptUserForNavigationToNewOrigins.name),
                  "true"},
             }}},
        },
        /*disabled_features=*/{features::kGlicWarming});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When kGlicPromptUserForNavigationToNewOrigins is enabled, we should not
// prompt twice for the same non-sensitive origin.
IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingUserPromptingBrowserTest,
                       ConfirmBlockedOriginWithUser_Nonsensitive) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL other_url = embedded_https_test_server().GetURL(
      "other.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // The user should be prompted due to
  // `kGlicPromptUserForNavigationToNewOrigins`.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", other_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({
    "navigationOrigin": $1,
    "forBlocklistedOrigin": false
  })",
          url::Origin::Create(other_url)))));

  // Start back at `start_url`, and try another x-origin navigation to
  // `other_url`.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Note: we expect *no* user confirmation dialog when navigating back to
  // `start_url`, because the actor has already actuated on that origin (the
  // `ClickTarget` call above) so such a confirmation would be confusing at best
  // (or misleading).

  // Now this should proceed without a user confirmation or a server
  // confirmation, since the user has already confirmed it.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), other_url));
}

// When kGlicPromptUserForNavigationToNewOrigins is enabled, we should not
// prompt twice even if the origin becomes sensitive during the task.
IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingUserPromptingBrowserTest,
                       ConfirmBlockedOriginWithUser_ComponentUpdate) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL eventually_sensitive = embedded_https_test_server().GetURL(
      "eventually-sensitive.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // The user should be prompted due to
  // `kGlicPromptUserForNavigationToNewOrigins`.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("setLink($1);", eventually_sensitive)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({
    "navigationOrigin": $1,
    "forBlocklistedOrigin": false
  })",
          url::Origin::Create(eventually_sensitive)))));

  base::FilePath proto_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("base_proto_v2.pb"));
  ASSERT_TRUE(SetUpOptimizationGuideComponentBlocklist(
      proto_path, "eventually-sensitive.example.com"));
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->MaybeUpdateHintsComponent({base::Version("2"), proto_path});

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Start back at `start_url`, and try another x-origin navigation to
  // `eventually_sensitive`.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Note: we expect *no* user confirmation dialog when navigating back to
  // `start_url`, because the actor has already actuated on that origin (the
  // `ClickTarget` call above) so such a confirmation would be confusing at best
  // (or misleading).

  // Now this should proceed without a user confirmation or a server
  // confirmation, since the user has already confirmed it.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), eventually_sensitive));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmSensitiveOriginWithUser_Denied) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sensitive_url = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", sensitive_url)));

  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(sensitive_url)))));

  // Should log that permission was *denied* once.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       OriginGatingNavigateAction) {
  const GURL start_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(cross_origin_url.spec())}));

  // Start on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Navigate to bar.com.
  std::unique_ptr<ToolRequest> navigate_x_origin =
      MakeNavigateRequest(*active_tab(), cross_origin_url.spec());
  // Navigate to foo.com page with a link to bar.com.
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), link_page_url.spec());
  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(navigate_x_origin, navigate_to_link_page, click_link),
      result1.GetCallback());
  ExpectOkResult(result1);

  // Test that navigation allowlist is not persisted across separate tasks.
  auto previous_id = actor_task().id();
  RunTestSequence(CloseGlic());
  StopAllTasks();
  OpenGlicAndCreateTask();
  ASSERT_NE(previous_id, actor_task().id());

  // Start on link page on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_page_url));
  // Click on full-page link to bar.com only.
  std::unique_ptr<ToolRequest> click_link_only =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result2;
  actor_task().Act(ToRequestList(click_link_only), result2.GetCallback());
  // Expect the navigation to be blocked by origin gating.
  ExpectErrorResult(result2,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       AddWritableMainframeOrigins) {
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(cross_origin_url.spec())}));

  // Start on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_page_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  PerformActionsFuture result1;
  actor_keyed_service().PerformActions(
      actor_task().id(),
      ToRequestList(MakeClickRequest(*active_tab(), gfx::Point(1, 1))),
      ActorTaskMetadata(), result1.GetCallback());
  ExpectErrorResult(result1,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);

  PerformActionsFuture result2;
  actor_keyed_service().PerformActions(
      actor_task().id(),
      ToRequestList(MakeClickRequest(*active_tab(), gfx::Point(1, 1))),
      ActorTaskMetadata::WithAddedWritableMainframeOriginsForTesting(
          {url::Origin::Create(cross_origin_url)}),
      result2.GetCallback());
  ExpectOkResult(result2);

  const auto ukm_entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::Actor_OriginGating::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  // First navigation was rejected
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[0], "ServerConfirmationResult", /*expected_value=*/
      static_cast<int64_t>(
          ExecutionEngine::ActorServerConfirmationResult::kRejected));
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[0], "EngineState", /*expected_value=*/
      static_cast<int64_t>(ExecutionEngine::State::kToolInvoke));
  // Second navigation did not record UKM since origin was allowlisted
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[1], "ServerConfirmationResult", /*expected_value=*/
      static_cast<int64_t>(
          ExecutionEngine::ActorServerConfirmationResult::kNotRequired));
  test_ukm_recorder()->ExpectEntryMetric(
      ukm_entries[1], "EngineState", /*expected_value=*/
      static_cast<int64_t>(ExecutionEngine::State::kToolInvoke));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       SensitiveNavigationNotAddedToAllowlist) {
  base::HistogramTester histogram_tester;
  const GURL start_url = embedded_https_test_server().GetURL(
      "www.example.com", "/actor/blank.html");
  const GURL sensitive_origin_url = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");
  const GURL sensitive_origin_link_url = embedded_https_test_server().GetURL(
      "sensitive.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(sensitive_origin_url.spec())}));
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "www.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(sensitive_origin_url.spec())}));

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Navigate to sensitive origin.
  std::unique_ptr<ToolRequest> navigate_to_sensitive =
      MakeNavigateRequest(*active_tab(), sensitive_origin_link_url.spec());
  // Clicks on full-page link to sensitive origin.
  std::unique_ptr<ToolRequest> click_link_same_origin =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));
  // Navigate from back to start
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), link_page_url.spec());
  // Clicks on full-page link to sensitive origin.
  std::unique_ptr<ToolRequest> click_link_x_origin =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_sensitive, click_link_same_origin,
                                 navigate_to_link_page, click_link_x_origin),
                   result.GetCallback());
  ExpectOkResult(result);

  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(sensitive_origin_url)))));

  // Trigger ExecutionEngine destructor for metrics.
  StopAllTasks();

  // Navigation gating should only be applied to the first navigation action.
  // All other navigations should not have gating.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Actor.NavigationGating.AppliedGate"),
      base::BucketsAre(base::Bucket(false, 3), base::Bucket(true, 1)));
  // Permission should have been explicitly granted twice. Once for each
  // navigation to sensitive origin.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     true, 1);
  // The allow-list should have 2 entries at the end of the task.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AllowListSize", 2,
                                     1);
  // The list of confirmed sensitive origins should have 1 entry.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.ConfirmedListSize2", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       SandboxedSiteDoesNotReprompt) {
  base::HistogramTester histogram_tester;
  const GURL sandboxed_sensitive_page = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/sandboxed_blank.html");
  const GURL blocked_page = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");
  const GURL normal_page_with_link = embedded_https_test_server().GetURL(
      "www.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(blocked_page.spec())}));

  // Start on sandboxed page.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), sandboxed_sensitive_page));
  OpenGlicAndCreateTask();

  // Perform some action on the sandboxed site
  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));
  // Try navigating away
  std::unique_ptr<ToolRequest> navigate_to_link =
      MakeNavigateRequest(*active_tab(), normal_page_with_link.spec());
  // Clicks on full-page link to go back to sandboxed page.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));
  ActResultFuture result;
  actor_task().Act(ToRequestList(click, navigate_to_link, click_link),
                   result.GetCallback());
  ExpectOkResult(result);

  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(blocked_page)))));

  // Trigger ExecutionEngine destructor for metrics.
  StopAllTasks();

  // Each actual navigation should not have applied the gate. The origin was
  // confirmed when during SafetyChecksForNextAction.
  histogram_tester.ExpectUniqueSample("Actor.NavigationGating.AppliedGate",
                                      false, 2);
  // Permission should have been explicitly granted once during
  // SafetyChecksForNextAction. The navigation to to `www.example.com` had
  // implicit permission via the tool request.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
  // The allow-list should have 2 entries at the end of the task.
  histogram_tester.ExpectUniqueSample("Actor.NavigationGating.AllowListSize", 2,
                                      1);
  // The list of confirmed sensitive origins should have 1 entry.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.ConfirmedListSize2", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       NavigationNotGatedWithStaticList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_allowed": [
        { "from": "*", "to": "[*.]example.com" },
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  // The navigation should log that gating was not applied due to the static
  // list.
  histogram_tester.ExpectUniqueSample("Actor.NavigationGating.AppliedGate",
                                      false, 1);

  histogram_tester.ExpectUniqueSample(kSameOriginSourceHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteSourceHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameOriginInitiatorHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteInitiatorHistogram, false, 1);
  // Should not log permission granted since the static list was used.
  histogram_tester.ExpectTotalCount("Actor.NavigationGating.PermissionGranted",
                                    0);
  // Second navigation should be allowed by static allowlist.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationInStaticAllowList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
     {
       "navigation_allowed": [
         { "from": "[*.]example.com", "to": "[*.]example.com" }
       ]
     }
   )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  // The navigation should be allowed due the site's membership in the static
  // allow list. (Note that if the site weren't on the allowlist, the navigation
  // would still be allowed since the navigation is same-origin and the site is
  // not on the blocklist.)
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationInStaticBlockList_Click) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
     {
       "navigation_blocked": [
         { "from": "[*.]example.com", "to": "[*.]example.com" }
       ]
     }
   )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));

  ClickTarget("#link", mojom::ActionResultCode::kActionsBlockedForSiteRisk);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationInStaticBlockList_Navigate) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/empty.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]example.com", "to": "[*.]example.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeNavigateRequest(*active_tab(), start_url.spec())),
      result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kActionsBlockedForSiteRisk);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AppliedGate", true,
                                     1);
  histogram_tester.ExpectUniqueSample(kSameOriginSourceHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteSourceHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(kSameOriginInitiatorHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteInitiatorHistogram, false, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       CrossOriginNavigationInStaticBlockListAndAllowList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_allowed": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ],
      "navigation_blocked": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", blocked_url)));
  ClickTarget("#link", mojom::ActionResultCode::kActionsBlockedForSiteRisk);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       StaticBlockOverridesDynamicList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  OpenGlicAndCreateTask();
  actor_task().GetExecutionEngine().AddWritableMainframeOrigins(
      {url::Origin::Create(blocked_url)});

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Navigate to blocked
  std::unique_ptr<ToolRequest> navigate_to_blocked =
      MakeNavigateRequest(*active_tab(), blocked_url.spec());

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_blocked), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kActionsBlockedForSiteRisk);

  StopAllTasks();

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AppliedGate", true,
                                     1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       StaticAllowListOverridesDynamicList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL allowed_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_allowed": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  OpenGlicAndCreateTask();

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Navigate to blocked
  std::unique_ptr<ToolRequest> navigate_to_allow =
      MakeNavigateRequest(*active_tab(), allowed_url.spec());

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_allow), result.GetCallback());
  ExpectOkResult(result);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(kSameOriginSourceHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteSourceHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameOriginInitiatorHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteInitiatorHistogram, false, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       NavigationBlockedByStaticList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", blocked_url)));
  ClickTarget("#link", mojom::ActionResultCode::kActionsBlockedForSiteRisk);

  // First navigation should be allowed due to same origin.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       NavigationBlockedByStaticList_CrossOriginIframe) {
  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": "blocked.example.com" }
      ]
    }
  )json");

  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/iframe.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("blocked.example.com", "/empty.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents(), "test", blocked_url));

  OpenGlicAndCreateTask();

  // No need to wait for the callback, since the tab is added to the controlled
  // set synchronously.
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  content::TestNavigationObserver observer(web_contents());
  ASSERT_TRUE(content::ExecJs(
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      content::JsReplace(R"(
      const a = document.createElement('a');
      a.target = "_parent";
      a.href = $1;
      document.body.appendChild(a);
      a.click();
      )",
                         blocked_url)));
  observer.Wait();

  // The navigation is blocked by the blocklist even though the initiator is
  // same-origin with the destination.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       NavigationWithOpaqueSourceOriginBlockedUnderWildcard) {
  base::HistogramTester histogram_tester;
  const GURL blocked_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": "[*.]example.com" }
      ]
    }
  )json");

  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  std::unique_ptr<ToolRequest> navigate_blocked =
      MakeNavigateRequest(*active_tab(), blocked_url.spec());

  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_blocked), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kActionsBlockedForSiteRisk);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       NavigateToSandboxedPageBlockedByStaticList) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sandboxed_url = embedded_https_test_server().GetURL(
      "foo.com", "/actor/sandbox_main_frame_csp.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", sandboxed_url)));
  ClickTarget("#link", mojom::ActionResultCode::kActionsBlockedForSiteRisk);

  // First navigation should be allowed due to same origin.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision2",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       BlocklistAppliesToTabAction) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "bad.example.com", "/actor/link.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
     {
       "navigation_blocked": [
         { "from": "*", "to": "[*.]bad.example.com" }
       ]
     }
)json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result;
  actor_task().Act(ToRequestList(click_link), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kActionsBlockedForSiteRisk);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       ActorContainerConfig_Navigation) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  optimization_guide::proto::LocationRule* rule =
      config_proto.add_location_rules();
  optimization_guide::proto::Site* site =
      rule->mutable_location()->mutable_site();
  site->set_protocol(optimization_guide::proto::Protocol::PROTOCOL_HTTPS);
  site->set_domain("example.com");
  optimization_guide::proto::RuleMetadata* metadata = rule->mutable_metadata();
  metadata->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  metadata->add_accessible_resources(
      optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);

  const GURL allowed_url = embedded_https_test_server().GetURL(
      "foo.example.com", "/actor/blank.html");
  const GURL start_page_url = embedded_https_test_server().GetURL(
      "example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(allowed_url.spec())}));

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_page_url));
  OpenGlicAndCreateTask();

  std::unique_ptr<ToolRequest> click_allowed_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  // Clicking link to go to the allowed page should work.
  PerformActionsFuture result1;
  actor_keyed_service().PerformActions(
      actor_task().id(), ToRequestList(click_allowed_link),
      ActorTaskMetadata::WithAgentContainerConfigForTesting(config_proto),
      result1.GetCallback());
  ExpectOkResult(result1);

  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  std::unique_ptr<ToolRequest> navigate_to_blocked_site =
      MakeNavigateRequest(*active_tab(), blocked_url.spec());

  // Should block even Navigate actions to a not-explicitly-allowed URL, only
  // need to supply container config on the first action.
  PerformActionsFuture result2;
  actor_keyed_service().PerformActions(
      actor_task().id(), ToRequestList(navigate_to_blocked_site),
      ActorTaskMetadata(), result2.GetCallback());
  ExpectErrorResult(result2,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingBrowserTest,
                       ActorContainerConfig_TaskStart) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  optimization_guide::proto::LocationRule* rule =
      config_proto.add_location_rules();
  optimization_guide::proto::Site* site =
      rule->mutable_location()->mutable_site();
  site->set_protocol(optimization_guide::proto::Protocol::PROTOCOL_HTTPS);
  site->set_domain("example.com");
  optimization_guide::proto::RuleMetadata* metadata = rule->mutable_metadata();
  metadata->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  metadata->add_accessible_resources(
      optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  // Add a NavigationSource that should be ignored.
  optimization_guide::proto::NavigationSource* nav_source =
      rule->add_navigation_sources();
  site = nav_source->mutable_source()->mutable_site();
  site->set_protocol(optimization_guide::proto::Protocol::PROTOCOL_HTTPS);
  site->set_domain("foo.com");

  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), blocked_url));
  OpenGlicAndCreateTask();

  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  // Should not be able to actuate the blocked page.
  PerformActionsFuture result;
  actor_keyed_service().PerformActions(
      actor_task().id(), ToRequestList(click),
      ActorTaskMetadata::WithAgentContainerConfigForTesting(config_proto),
      result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);
}

class ExecutionEngineOriginGatingParamBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<std::tuple<
          /*confirm_navigation_to_new_origins_enabled=*/bool,
          /*prompt_user_for_navigation_to_new_origins_enabled=*/bool,
          /*allow_implicit_tool_origin_grants=*/bool>> {
 public:
  ExecutionEngineOriginGatingParamBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kGlicCrossOriginNavigationGating,
             {{
                 {"confirm_navigation_to_new_origins",
                  confirm_navigation_to_new_origins_enabled() ? "true"
                                                              : "false"},
                 {"prompt_user_for_navigation_to_new_origins",
                  prompt_user_for_navigation_to_new_origins_enabled()
                      ? "true"
                      : "false"},
                 {"allow_implicit_tool_origin_grants",
                  allow_implicit_tool_origin_grants() ? "true" : "false"},
             }}},
        },
        /*disabled_features=*/{});
  }
  ~ExecutionEngineOriginGatingParamBrowserTest() override = default;

  bool confirm_navigation_to_new_origins_enabled() {
    return std::get<0>(GetParam());
  }
  bool prompt_user_for_navigation_to_new_origins_enabled() {
    return std::get<1>(GetParam());
  }
  bool allow_implicit_tool_origin_grants() { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       PromptUserForNewOrigin) {
  base::HistogramTester histogram_tester;
  if (!prompt_user_for_navigation_to_new_origins_enabled()) {
    GTEST_SKIP() << "prompt_user_for_navigation_to_new_origins disabled "
                    "already tested in ExecutionEngineOriginGatingBrowserTest.";
  }

  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Now we expect the navigation to trigger a user confirmation instead.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": false})",
          url::Origin::Create(second_url)))));

  // Trigger ExecutionEngine destructor for metrics.
  StopAllTasks();

  // Should have added both origins to the allowlist.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AllowListSize", 2,
                                     1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       ConfirmWithUserForTabAction) {
  base::HistogramTester histogram_tester;
  const GURL start_url = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");

  OpenGlicAndCreateTask();

  // Mock IPC response will always confirm the request.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  // Start on sensitive.example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result;
  actor_task().Act(ToRequestList(click_link), result.GetCallback());
  ExpectOkResult(result);

  // Trigger ExecutionEngine destructor for metrics.
  StopAllTasks();

  // There should be a single confirmation.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     true, 1);
  // The allow-list should have 1 entry at the end of the task.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AllowListSize", 1,
                                     1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       ImplicitToolOriginGrants) {
  const GURL start_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(cross_origin_url.spec())}));

  // Start on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Navigate to bar.com.
  std::unique_ptr<ToolRequest> navigate_x_origin =
      MakeNavigateRequest(*active_tab(), cross_origin_url.spec());
  // Navigate to foo.com page with a link to bar.com.
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), link_page_url.spec());
  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(navigate_x_origin, navigate_to_link_page, click_link),
      result.GetCallback());
  if (allow_implicit_tool_origin_grants()) {
    ExpectOkResult(result);
  } else {
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kTriggeredNavigationBlocked);
  }
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       ImplicitToolOriginGrantWithTaskMetadata) {
  const GURL start_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(cross_origin_url.spec())}));

  // Start on foo.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Navigate to bar.com.
  std::unique_ptr<ToolRequest> navigate_x_origin =
      MakeNavigateRequest(*active_tab(), cross_origin_url.spec());
  // Navigate to foo.com page with a link to bar.com.
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), link_page_url.spec());
  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  PerformActionsFuture result;
  actor_keyed_service().PerformActions(
      actor_task().id(),
      ToRequestList(navigate_x_origin, navigate_to_link_page, click_link),
      ActorTaskMetadata::WithAddedWritableMainframeOriginsForTesting(
          {url::Origin::Create(start_url),
           url::Origin::Create(cross_origin_url)}),
      result.GetCallback());
  ExpectOkResult(result);
}

// Tuple values are:
// (confirm_navigation_to_new_origins,
//  prompt_user_for_navigation_to_new_origins,
//  allow_implicit_tool_origin_grants).
INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingParamBrowserTest,
                         testing::Values(std::make_tuple(false, false, true),
                                         std::make_tuple(true, true, true),
                                         std::make_tuple(true, false, false)),
                         [](auto& info) {
                           if (!std::get<0>(info.param)) {
                             return "NavigationConfirmDisabled";
                           }
                           if (std::get<1>(info.param)) {
                             return "PromptToConfirmNavigation";
                           }
                           if (!std::get<2>(info.param)) {
                             return "ImplicitToolOriginGrantsDisabled";
                           }
                           NOTREACHED();
                         });

class ExecutionEngineOriginGatingSafetyDisabledBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExecutionEngineOriginGatingBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(actor::switches::kDisableActorSafetyChecks);
  }
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingSafetyDisabledBrowserTest,
                       IgnoreSensitiveUrlList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sensitive_url = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  std::unique_ptr<ToolRequest> navigate_to_sensitive =
      MakeNavigateRequest(*active_tab(), sensitive_url.spec());

  // Execute the navigation action.
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_sensitive), result.GetCallback());

  // The navigation should succeed because the safety checks are disabled.
  ExpectOkResult(result);

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), sensitive_url);
}

class ExecutionEngineSiteGatingBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineSiteGatingBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kGlicCrossOriginNavigationGating,
             {{
                 {"confirm_navigation_to_new_origins", "true"},
                 {"prompt_user_for_navigation_to_new_origins", "false"},
                 {"gate_on_site_not_origin",
                  should_gate_by_site() ? "true" : "false"},
             }}},
        },
        /*disabled_features=*/{});
  }
  ~ExecutionEngineSiteGatingBrowserTest() override = default;

  bool should_gate_by_site() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest,
                       ConfirmNavigation_SameOrigin) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Same origin source should never trigger gating
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest,
                       ConfirmNavigation_CrossOrigin_Denied) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL same_site = embedded_https_test_server().GetURL(
      "other.example.com", "/actor/link.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Cross origin but same site source should trigger when we're gating on
  // origin
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", same_site)));
  ClickTarget("#link",
              should_gate_by_site()
                  ? mojom::ActionResultCode::kOk
                  : mojom::ActionResultCode::kTriggeredNavigationBlocked);

  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     false, should_gate_by_site() ? 0 : 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest,
                       ConfirmNavigation_CrossSite_Denied) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL cross_site =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Cross site source will always trigger gating
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", cross_site)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     false, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest,
                       SensitiveSiteListAlwaysUsesOrigin) {
  base::HistogramTester histogram_tester;
  if (!should_gate_by_site()) {
    GTEST_SKIP() << "SensitiveSiteList already tested in "
                    "ExecutionEngineOriginGatingBrowserTest.";
  }
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sensitive_url = embedded_https_test_server().GetURL(
      "sensitive.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  ASSERT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", sensitive_url)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(sensitive_url)))));

  // Should log that permission was *denied* once.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     false, 1);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest, PerTaskAllowlist) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  const GURL other_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  const GURL other_url_same_site =
      embedded_https_test_server().GetURL("other.foo.com", "/actor/blank.html");
  const GURL cross_site_url_with_link = embedded_https_test_server().GetURL(
      "bar.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(other_url_same_site.spec())}));

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Navigate to foo.com.
  std::unique_ptr<ToolRequest> navigate_x_origin =
      MakeNavigateRequest(*active_tab(), other_url.spec());
  // Navigate to bar.com page with a link to other.foo.com.
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), cross_site_url_with_link.spec());
  // Clicks on full-page link to other.foo.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(navigate_x_origin, navigate_to_link_page, click_link),
      result.GetCallback());
  if (should_gate_by_site()) {
    ExpectOkResult(result);
  } else {
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kTriggeredNavigationBlocked);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineSiteGatingBrowserTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "GateBySite" : "GateByOrigin";
                         });

class ExecutionEngineBlocklistDisabledBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase {
 public:
  ExecutionEngineBlocklistDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic, {}},
            {features::kGlicActor,
             {{features::kGlicActorPolicyControlExemption.name, "true"}}},
            {kGlicCrossOriginNavigationGating,
             {
                 {"confirm_navigation_to_new_origins", "false"},
                 {"enforce_component_updater_block_list_entries", "false"},
             }},
        },
        /*disabled_features=*/{});
  }
  ~ExecutionEngineBlocklistDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineBlocklistDisabledBrowserTest,
                       NavigateToBlockedUrlAllowed) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": "[*.]foo.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Attempt to navigate to the blocked URL by setting link and clicking
  // it.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", blocked_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  // It should succeed because component updater blocklist is disabled by flag.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), blocked_url);
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineBlocklistDisabledBrowserTest,
                       ClickOnPageFromBlockedUrlAllowed) {
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": "[*.]foo.com" }
      ]
    }
  )json");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), blocked_url));
  OpenGlicAndCreateTask();

  std::unique_ptr<ToolRequest> click_on_page =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result;
  actor_task().Act(ToRequestList(click_on_page), result.GetCallback());
  ExpectOkResult(result);
}

class ExecutionEngineOriginGatingDarkLaunchBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineOriginGatingDarkLaunchBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic, {}},
            {features::kGlicActor,
             {{features::kGlicActorPolicyControlExemption.name, "true"}}},
            {kGlicCrossOriginNavigationGating,
             {
                 {"confirm_navigation_to_new_origins",
                  BlockingConfirmationsEnabled() ? "true" : "false"},
                 {"confirm_navigation_to_new_origins_dark_launch", "true"},
             }},
        },
        /*disabled_features=*/{features::kGlicWarming});
  }
  ~ExecutionEngineOriginGatingDarkLaunchBrowserTest() override = default;

  bool BlockingConfirmationsEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingDarkLaunchBrowserTest,
                       NavigationConfirmation_DelayedResponse) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      kSetUpDelayedNavigationConfirmationRequestHandler));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  // Trigger navigation.
  std::optional<int> dom_node_id =
      content::GetDOMNodeId(*main_frame(), "#link");
  ASSERT_TRUE(dom_node_id);
  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*main_frame(), dom_node_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(click), result.GetCallback());

  if (BlockingConfirmationsEnabled()) {
    // In active gating mode, the click action is deferred and not completed
    // yet.
    EXPECT_FALSE(result.IsReady());
    EXPECT_NE(web_contents()->GetLastCommittedURL(), second_url);
  } else {
    // In dark launch mode, the click action completes immediately.
    ExpectOkResult(result);
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), second_url);
  }

  // Verify the background navigation confirmation request was sent to the
  // client.
  RunTestSequence(WaitUntilPendingConfirmationRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "taskId": $2})",
          url::Origin::Create(second_url), actor_task().id().value()))));

  // Explicitly verify that the confirmation has NOT responded yet (no histogram
  // recorded).
  histogram_tester.ExpectTotalCount("Actor.NavigationGating.PermissionGranted",
                                    0);

  // Respond to the pending request to unblock/clean up.
  RunTestSequence(RespondToPendingRequest(true));

  if (BlockingConfirmationsEnabled()) {
    // In active gating mode, the click action now unblocks and completes
    // successfully.
    ExpectOkResult(result);
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), second_url);
  }

  // Wait and verify that the confirmation has now responded successfully.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester
               .GetAllSamples("Actor.NavigationGating.PermissionGranted")
               .size() == 1;
  }));
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingDarkLaunchBrowserTest,
                         testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "ConfirmOriginsEnabled"
                                             : "ConfirmOriginsDisabled";
                         });

class ExecutionEngineOriginGatingSlowResponseBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase {
 public:
  ExecutionEngineOriginGatingSlowResponseBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlicActor,
             {{features::kGlicActorPolicyControlExemption.name, "true"},
              {features::kGlicActorPageStabilityTimeout.name, "300ms"},
              {features::kActorObservationDelayTimeout.name, "1s"}}},
        },
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    CHECK(!embedded_https_test_server().Started());
    response_manager_ =
        std::make_unique<net::test_server::ControllableHttpResponseManager>(
            &embedded_https_test_server(), "/slow");
    ExecutionEngineOriginGatingBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  std::unique_ptr<net::test_server::ControllableHttpResponseManager>
      response_manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that navigations are subject to safety checks even if the relevant
// action sequence times out (and is therefore cancelled).
IN_PROC_BROWSER_TEST_F(ExecutionEngineOriginGatingSlowResponseBrowserTest,
                       SlowResponseDoesntBypassNavGating) {
  base::HistogramTester histogram_tester;

  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("setLink($1);",
                         embedded_https_test_server().GetURL(
                             "sensitive.example.com", "/slow"))));

  ActResultFuture act_result;
  content::TestNavigationObserver nav_observer(web_contents());
  actor_task().Act(ToRequestList(MakeClickRequest(
                       *main_frame(),
                       content::GetDOMNodeId(*main_frame(), "#link").value())),
                   act_result.GetCallback());
  // No handler has been registered; wait for the observation delay to cancel
  // the action.
  ASSERT_TRUE(act_result.Wait());

  std::unique_ptr<net::test_server::ControllableHttpResponse> slow_response =
      response_manager_->WaitForRequest();
  slow_response->Send(net::HTTP_OK);
  slow_response->Done();
  nav_observer.Wait();

  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      /*sample=*/ExecutionEngine::GatingDecision::kNeedsAsyncCheck,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Actor.NavigationGating.AppliedGate",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", /*sample=*/false,
      /*expected_bucket_count=*/1);
}

enum class UiPromptType {
  kNone,
  kNavConfirmation,
  kUserConfirmationDialog,
};

class OutOfTurnNavigationTestBase
    : public ExecutionEngineOriginGatingBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    ExecutionEngineOriginGatingBrowserTestBase::SetUpOnMainThread();
    ParseSafetyListsForTesting(SafetyListManager::GetInstance(), R"json(
      {
        "navigation_blocked": [
          { "from": "*", "to": "bar.com" }
        ]
      }
    )json");
  }

  // Sets up mock web client handlers for confirmation requests. Out-of-turn
  // navigations are treated like any other navigation and may trigger a user
  // confirmation dialog or a navigation confirmation request depending on
  // the target origin, requiring handlers to resolve pending requests.
  void SetUpUiPrompt(UiPromptType ui_prompt_type) {
    if (ui_prompt_type == UiPromptType::kNavConfirmation) {
      RunTestSequence(CreateMockWebClientRequest(
          kSetUpDelayedNavigationConfirmationRequestHandler));
    } else if (ui_prompt_type == UiPromptType::kUserConfirmationDialog) {
      RunTestSequence(CreateMockWebClientRequest(
          kSetUpDelayedUserConfirmationDialogRequestHandler));
    }
  }

  void WaitForUiPromptIfNeeded(UiPromptType ui_prompt_type,
                               const GURL& target_url) {
    if (ui_prompt_type == UiPromptType::kUserConfirmationDialog) {
      RunTestSequence(WaitUntilPendingConfirmationRequest(
          base::test::ParseJsonDict(content::JsReplace(
              R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
              url::Origin::Create(target_url)))));
    } else if (ui_prompt_type == UiPromptType::kNavConfirmation) {
      RunTestSequence(WaitUntilPendingConfirmationRequest(
          base::test::ParseJsonDict(content::JsReplace(
              R"({"navigationOrigin": $1, "taskId": $2})",
              url::Origin::Create(target_url), actor_task().id().value()))));
    }
  }

  void WaitAndResolveUiPromptIfNeeded(UiPromptType ui_prompt_type,
                                      const GURL& target_url,
                                      bool expects_permission_granted) {
    if (ui_prompt_type != UiPromptType::kNone) {
      WaitForUiPromptIfNeeded(ui_prompt_type, target_url);
      RunTestSequence(RespondToPendingRequest(expects_permission_granted));
    }
  }
};

struct OutOfTurnTestParam {
  std::string_view test_name;
  std::string_view target_host;
  UiPromptType ui_prompt_type = UiPromptType::kNone;
  bool expects_permission_granted = false;
};

class OutOfTurnNavigationBrowserTest
    : public OutOfTurnNavigationTestBase,
      public testing::WithParamInterface<OutOfTurnTestParam> {};

IN_PROC_BROWSER_TEST_P(OutOfTurnNavigationBrowserTest,
                       IdleEngine_OutOfTurnNavigation) {
  const OutOfTurnTestParam& param = GetParam();
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL target_url = embedded_https_test_server().GetURL(
      param.target_host, "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());
  CHECK(actor_task().HasTab(active_tab()->GetHandle()));

  SetUpUiPrompt(param.ui_prompt_type);

  content::TestNavigationObserver nav_observer(web_contents());

  // Trigger out-of-turn navigation via JavaScript.
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("window.location.href = $1;", target_url)));

  WaitAndResolveUiPromptIfNeeded(param.ui_prompt_type, target_url,
                                 param.expects_permission_granted);

  nav_observer.Wait();

  if (param.expects_permission_granted) {
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), target_url);
  } else {
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
  }
}

IN_PROC_BROWSER_TEST_P(OutOfTurnNavigationBrowserTest,
                       PausedTask_OutOfTurnNavigation) {
  const OutOfTurnTestParam& param = GetParam();
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL target_url = embedded_https_test_server().GetURL(
      param.target_host, "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());
  CHECK(actor_task().HasTab(active_tab()->GetHandle()));

  // Pause the task so that it is in user control / paused state.
  actor_task().Pause(/*from_actor=*/true);
  ASSERT_TRUE(actor_task().IsUnderUserControl());
  ASSERT_FALSE(actor_task().IsActingOnTab(active_tab()->GetHandle()));
  ASSERT_TRUE(actor_task().HasTab(active_tab()->GetHandle()));

  SetUpUiPrompt(param.ui_prompt_type);

  content::TestNavigationObserver nav_observer(web_contents());

  // Trigger out-of-turn navigation via JavaScript while paused.
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("window.location.href = $1;", target_url)));

  WaitAndResolveUiPromptIfNeeded(param.ui_prompt_type, target_url,
                                 param.expects_permission_granted);

  nav_observer.Wait();

  if (param.expects_permission_granted) {
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), target_url);
  } else {
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
  }
}

// TODO(crbug.com/482434165): Flaky test on win-asan builder.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_InterleavedAction_OutOfTurnNavigation \
  DISABLED_InterleavedAction_OutOfTurnNavigation
#else
#define MAYBE_InterleavedAction_OutOfTurnNavigation \
  InterleavedAction_OutOfTurnNavigation
#endif
IN_PROC_BROWSER_TEST_P(OutOfTurnNavigationBrowserTest,
                       MAYBE_InterleavedAction_OutOfTurnNavigation) {
  const OutOfTurnTestParam& param = GetParam();
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL target_url = embedded_https_test_server().GetURL(
      param.target_host, "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());
  CHECK(actor_task().HasTab(active_tab()->GetHandle()));

  SetUpUiPrompt(param.ui_prompt_type);

  content::TestNavigationObserver nav_observer(web_contents());

  int dom_node_id = content::GetDOMNodeId(*main_frame(), "body").value();
  std::unique_ptr<ToolRequest> click_on_page =
      MakeClickRequest(*main_frame(), dom_node_id);

  // 1. Trigger out-of-turn navigation via JS.
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("window.location.href = $1;", target_url)));

  // 2. While navigation is deferred/pending, execute a new action via Act().
  ActResultFuture result;
  actor_task().Act(ToRequestList(click_on_page), result.GetCallback());

  // 3. Resolve the UI prompt for the out-of-turn navigation if required.
  WaitAndResolveUiPromptIfNeeded(param.ui_prompt_type, target_url,
                                 param.expects_permission_granted);

  // 4. Verify new action completes and navigation finishes according to
  // expectation.
  nav_observer.Wait();

  if (param.expects_permission_granted) {
    ExpectErrorResult(result, mojom::ActionResultCode::kFrameWentAway);
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), target_url);
  } else {
    ExpectOkResult(result);
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
  }
}

constexpr OutOfTurnTestParam kOutOfTurnTestParams[] = {
    {.test_name = "SameOriginAllowed",
     .target_host = "example.com",
     .ui_prompt_type = UiPromptType::kNone,
     .expects_permission_granted = true},
    {.test_name = "NavConfirmationAllowed",
     .target_host = "foo.com",
     .ui_prompt_type = UiPromptType::kNavConfirmation,
     .expects_permission_granted = true},
    {.test_name = "NavConfirmationDenied",
     .target_host = "foo.com",
     .ui_prompt_type = UiPromptType::kNavConfirmation,
     .expects_permission_granted = false},
    {.test_name = "UserDialogAllowed",
     .target_host = "sensitive.example.com",
     .ui_prompt_type = UiPromptType::kUserConfirmationDialog,
     .expects_permission_granted = true},
    {.test_name = "UserDialogDenied",
     .target_host = "sensitive.example.com",
     .ui_prompt_type = UiPromptType::kUserConfirmationDialog,
     .expects_permission_granted = false},
    {.test_name = "Blocklisted",
     .target_host = "bar.com",
     .ui_prompt_type = UiPromptType::kNone,
     .expects_permission_granted = false},
};

INSTANTIATE_TEST_SUITE_P(All,
                         OutOfTurnNavigationBrowserTest,
                         testing::ValuesIn(kOutOfTurnTestParams),
                         [](const auto& info) {
                           return std::string(info.param.test_name);
                         });

struct OutOfTurnTaskStoppedTestParam {
  std::string_view test_name;
  std::string_view target_host;
  UiPromptType ui_prompt_type = UiPromptType::kNone;
};

class OutOfTurnTaskStoppedBrowserTest
    : public OutOfTurnNavigationTestBase,
      public testing::WithParamInterface<OutOfTurnTaskStoppedTestParam> {};

IN_PROC_BROWSER_TEST_P(OutOfTurnTaskStoppedBrowserTest,
                       TaskStopped_DropsPendingNavigation) {
  const OutOfTurnTaskStoppedTestParam& param = GetParam();
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL target_url = embedded_https_test_server().GetURL(
      param.target_host, "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      base::DoNothing());
  CHECK(actor_task().HasTab(active_tab()->GetHandle()));

  SetUpUiPrompt(param.ui_prompt_type);

  content::TestNavigationObserver nav_observer(web_contents());

  // 1. Trigger out-of-turn navigation via JS.
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("window.location.href = $1;", target_url)));

  // 2. If it requires UI prompt, wait until request is pending in WebUI.
  WaitForUiPromptIfNeeded(param.ui_prompt_type, target_url);

  // 3. Stop the task while navigation is deferred.
  actor_keyed_service().StopTask(actor_task().id(),
                                 ActorTask::StoppedReason::kStoppedByUser);

  // 4. Verify navigation is dropped and canceled immediately without hanging.
  nav_observer.Wait();
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
}

constexpr OutOfTurnTaskStoppedTestParam kOutOfTurnTaskStoppedTestParams[] = {
    {.test_name = "NavConfirmation",
     .target_host = "foo.com",
     .ui_prompt_type = UiPromptType::kNavConfirmation},
    {.test_name = "UserDialog",
     .target_host = "sensitive.example.com",
     .ui_prompt_type = UiPromptType::kUserConfirmationDialog},
};

INSTANTIATE_TEST_SUITE_P(All,
                         OutOfTurnTaskStoppedBrowserTest,
                         testing::ValuesIn(kOutOfTurnTaskStoppedTestParams),
                         [](const auto& info) {
                           return std::string(info.param.test_name);
                         });

struct LocalhostTestParam {
  const char* test_name;
  const char* host;
};

class ExecutionEngineLocalhostUrlGatingBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<LocalhostTestParam> {
 public:
  ExecutionEngineLocalhostUrlGatingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kGlicActorLocalhostIsSensitive);
  }

  ~ExecutionEngineLocalhostUrlGatingBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineLocalhostUrlGatingBrowserTest,
                       LocalhostPageActionAllowedByUser) {
  const GURL localhost_url =
      embedded_test_server()->GetURL(GetParam().host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), localhost_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  WaitTool::SetNoDelayForTesting();
  std::unique_ptr<ToolRequest> tool_request = MakeWaitRequest(active_tab());
  ASSERT_TRUE(tool_request->RequiresUrlCheckInCurrentTab());

  ActResultFuture result;
  actor_task().Act(ToRequestList(tool_request), result.GetCallback());
  ExpectOkResult(result);

  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(localhost_url)))));

  // Subsequent check for the same origin is allowed via cache without
  // re-prompting.
  std::unique_ptr<ToolRequest> tool_request2 = MakeWaitRequest(active_tab());
  ActResultFuture result2;
  actor_task().Act(ToRequestList(tool_request2), result2.GetCallback());
  ExpectOkResult(result2);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineLocalhostUrlGatingBrowserTest,
                       LocalhostPageActionDeniedByUser) {
  const GURL localhost_url =
      embedded_test_server()->GetURL(GetParam().host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), localhost_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  WaitTool::SetNoDelayForTesting();
  std::unique_ptr<ToolRequest> tool_request = MakeWaitRequest(active_tab());
  ASSERT_TRUE(tool_request->RequiresUrlCheckInCurrentTab());

  ActResultFuture result;
  actor_task().Act(ToRequestList(tool_request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineLocalhostUrlGatingBrowserTest,
                       LocalhostNavigateAllowedByUser) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  const GURL localhost_url =
      embedded_test_server()->GetURL(GetParam().host, "/title1.html");
  std::unique_ptr<ToolRequest> tool_request =
      MakeNavigateRequest(*active_tab(), localhost_url.spec());

  ActResultFuture result;
  actor_task().Act(ToRequestList(tool_request), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), localhost_url);

  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(localhost_url)))));
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineLocalhostUrlGatingBrowserTest,
                       LocalhostNavigateDeniedByUser) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  const GURL localhost_url =
      embedded_test_server()->GetURL(GetParam().host, "/title1.html");
  std::unique_ptr<ToolRequest> tool_request =
      MakeNavigateRequest(*active_tab(), localhost_url.spec());

  ActResultFuture result;
  actor_task().Act(ToRequestList(tool_request), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
}

constexpr LocalhostTestParam kLocalhostTestParams[] = {
    {.test_name = "LocalhostDomain", .host = "localhost"},
    {.test_name = "Ipv4Loopback", .host = "127.0.0.1"},
    {.test_name = "SubdomainLocalhost", .host = "foo.localhost"},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExecutionEngineLocalhostUrlGatingBrowserTest,
    testing::ValuesIn(kLocalhostTestParams),
    [](const testing::TestParamInfo<LocalhostTestParam>& info) {
      return info.param.test_name;
    });

}  // namespace actor
