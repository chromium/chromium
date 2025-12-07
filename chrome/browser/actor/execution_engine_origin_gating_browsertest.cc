// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_switches.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/safety_list_manager.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace actor {

namespace {

constexpr char kHandleUserConfirmationDialogTempl[] =
    R"js(
  (() => {
    window.userConfirmationDialogRequestData = new Promise(resolve => {
      client.browser.selectUserConfirmationDialogRequestHandler().subscribe(
        request => {
          // Response will be verified in C++ callback below.
          request.onDialogClosed({
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

constexpr char kHandleNavigationConfirmationTempl[] =
    R"js(
  (() => {
    window.navigationConfirmationRequestData = new Promise(resolve => {
      client.browser.selectNavigationConfirmationRequestHandler()
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

}  // namespace

class ExecutionEngineOriginGatingBrowserTestBase
    : public glic::NonInteractiveGlicTest {
 public:
  ExecutionEngineOriginGatingBrowserTestBase() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic, {}},
            {features::kGlicActor, {}},
            {features::kTabstripComboButton, {}},
            {kGlicCrossOriginNavigationGating,
             {{
                 {"confirm_navigation_to_new_origins", "true"},
             }}},
        },
        /*disabled_features=*/{features::kGlicWarming});
  }
  ~ExecutionEngineOriginGatingBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    actor_keyed_service().GetPolicyChecker().SetActOnWebForTesting(true);

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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    glic::test::InteractiveGlicTest::SetUpCommandLine(command_line);
    SetUpBlocklist(command_line, "blocked.example.com");
  }

  virtual bool multi_instance_enabled() {
    return base::FeatureList::IsEnabled(features::kGlicMultiInstance);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  InteractiveTestApi::MultiStep CreateMockWebClientRequest(
      const std::string_view handle_dialog_js) {
    return InAnyContext(WithElement(
        glic::test::kGlicContentsElementId,
        [handle_dialog_js](::ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          ASSERT_TRUE(content::ExecJs(glic_contents, handle_dialog_js));
        }));
  }

  InteractiveTestApi::MultiStep VerifyUserConfirmationDialogRequest(
      const base::Value::Dict& expected_request) {
    static constexpr char kGetUserConfirmationDialogRequest[] =
        R"js(
          (() => {
            return window.userConfirmationDialogRequestData;
          })();
        )js";
    return VerifyWebClientRequest(kGetUserConfirmationDialogRequest,
                                  expected_request);
  }

  InteractiveTestApi::MultiStep VerifyNavigationConfirmationRequest(
      const base::Value::Dict& expected_request) {
    static constexpr char kGetNavigationConfirmationRequestData[] =
        R"js(
          (() => {
            return window.navigationConfirmationRequestData;
          })();
        )js";
    return VerifyWebClientRequest(kGetNavigationConfirmationRequestData,
                                  expected_request);
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }
  ActorKeyedService& actor_keyed_service() {
    return *ActorKeyedService::Get(browser()->profile());
  }
  ActorTask& actor_task() { return *actor_keyed_service().GetTask(task_id_); }
  tabs::TabInterface* active_tab() {
    return browser()->tab_strip_model()->GetActiveTab();
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
      const base::Value::Dict& expected_request) {
    return InAnyContext(WithElement(
        glic::test::kGlicContentsElementId,
        [&, get_request_js](::ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          auto eval_result = content::EvalJs(glic_contents, get_request_js);
          const auto& actual_request = eval_result.ExtractDict();
          ASSERT_EQ(expected_request, actual_request);
        }));
  }

  void OpenGlicAndCreateTask() {
    RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached));
    TrackGlicInstanceWithTabIndex(
        InProcessBrowserTest::browser()->tab_strip_model()->active_index());
    base::test::TestFuture<
        base::expected<int32_t, glic::mojom::CreateTaskErrorReason>>
        create_task_future;
    if (multi_instance_enabled()) {
      ASSERT_TRUE(GetGlicInstanceImpl());
      GetGlicInstanceImpl()->CreateTask(nullptr, nullptr,
                                        create_task_future.GetCallback());
    } else {
      glic::GlicKeyedService* service = glic::GlicKeyedService::Get(
          InProcessBrowserTest::browser()->profile());
      service->CreateTask(service->GetWeakPtr(), nullptr,
                          create_task_future.GetCallback());
    }
    auto result = create_task_future.Get();
    ASSERT_TRUE(result.has_value());
    task_id_ = TaskId(result.value());
  }

 protected:
  base::HistogramTester histogram_tester_for_init_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TaskId task_id_;
  base::ScopedTempDir temp_dir_;
};

class ExecutionEngineOriginGatingBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineOriginGatingBrowserTest() {
    if (multi_instance_enabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicMultiInstance,
                                glic::mojom::features::kGlicMultiTab},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kGlicMultiInstance,
                                 glic::mojom::features::kGlicMultiTab});
    }
  }
  ~ExecutionEngineOriginGatingBrowserTest() override = default;

  bool multi_instance_enabled() override { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmNavigationToNewOrigin_Granted) {
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
  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(second_url).GetDebugString())
          .Set("taskId", actor_task().id().value());
  RunTestSequence(VerifyNavigationConfirmationRequest(expected_request));

  // The first navigation should log that gating was not applied. The second
  // should log that gating was applied.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", true, 1);
  // Should log that there was a cross-origin navigation and a cross-site
  // navigation.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossOrigin", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossOrigin", true, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossSite", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossSite", true, 1);
  // Should log that permission was *granted* once.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmNavigationToNewOrigin_Denied) {
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
  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(second_url).GetDebugString())
          .Set("taskId", actor_task().id().value());
  RunTestSequence(VerifyNavigationConfirmationRequest(expected_request));

  // Should log that permission was *denied* once.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmBlockedOriginWithUser_Granted) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", blocked_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);
  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(blocked_url).GetDebugString())
          .Set("forBlocklistedOrigin", true);
  RunTestSequence(VerifyUserConfirmationDialogRequest(expected_request));

  // The first navigation should log that gating was not applied. The second
  // should log that gating was applied.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", true, 1);
  // Should log that there was a cross-origin navigation and a cross-site
  // navigation.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossOrigin", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossOrigin", true, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossSite", false, 2);
  // Should log that permission was *granted* once.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmBlockedOriginWithUser_Denied) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", blocked_url)));

  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(blocked_url).GetDebugString())
          .Set("forBlocklistedOrigin", true);
  RunTestSequence(VerifyUserConfirmationDialogRequest(expected_request));

  // Should log that permission was *denied* once.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       OriginGatingNavigateAction) {
  const GURL start_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com", base::StrCat({"/actor/link_full_page.html?href=",
                               EncodeURI(cross_origin_url.spec())}));

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
  actor_keyed_service().ResetForTesting();
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

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       AddWritableMainframeOrigins) {
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com", base::StrCat({"/actor/link_full_page.html?href=",
                               EncodeURI(cross_origin_url.spec())}));

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
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       BlockedNavigationNotAddedToAllowlist) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "www.example.com", "/actor/blank.html");
  const GURL blocked_origin_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  const GURL blocked_origin_link_url = embedded_https_test_server().GetURL(
      "blocked.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    EncodeURI(blocked_origin_url.spec())}));
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "www.example.com", base::StrCat({"/actor/link_full_page.html?href=",
                                       EncodeURI(blocked_origin_url.spec())}));

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Navigate to blocked origin.
  std::unique_ptr<ToolRequest> navigate_to_blocked =
      MakeNavigateRequest(*active_tab(), blocked_origin_link_url.spec());
  // Clicks on full-page link to blocked origin.
  std::unique_ptr<ToolRequest> click_link_same_origin =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));
  // Navigate from back to start
  std::unique_ptr<ToolRequest> navigate_to_link_page =
      MakeNavigateRequest(*active_tab(), link_page_url.spec());
  // Clicks on full-page link to blocked origin.
  std::unique_ptr<ToolRequest> click_link_x_origin =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_blocked, click_link_same_origin,
                                 navigate_to_link_page, click_link_x_origin),
                   result.GetCallback());
  ExpectOkResult(result);

  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(blocked_origin_url).GetDebugString())
          .Set("forBlocklistedOrigin", true);
  VerifyUserConfirmationDialogRequest(expected_request);

  // Trigger ExecutionEngine destructor for metrics.
  actor_keyed_service().ResetForTesting();

  // Navigation gating should only be applied to the first navigation action.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", true, 1);
  // All other navigations should not have gating
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", false, 3);
  // Permission should have been explicitly granted twice. Once for each
  // navigation to blocked.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true, 1);
  // The allow-list should have 2 entries at the end of the task.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AllowListSize", 2, 1);
  // The list of confirmed sensitive origins should have 1 entry.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.ConfirmedListSize", 1, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigationNotGatedWithStaticList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.AppliedGate", false, 1);
  // Should log that there was one same-site navigation and one cross-site
  // navigation.
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.CrossSite", true, 1);
  // Should not log permission granted since the static list was used.
  histogram_tester_for_init_.ExpectTotalCount(
      "Actor.NavigationGating.PermissionGranted", 0);
  // Second navigation should be allowed by static allowlist.
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationInStaticAllowList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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

  // The navigation should be allowed due to same origin, even though it's also
  // in the static allow list.
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationInStaticBlockList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
  {
    "navigation_blocked": [
      { "from": "example.com", "to": "[*.]example.com" }
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

  // The navigation should be allowed due to same origin, even though it's also
  // in the static block list.
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationBlockedByWildcardSource) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
  {
    "navigation_blocked": [
      { "from": "*", "to": "[*.]example.com" }
    ]
  }
)json");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // The navigation should be blocked by the static blocklist due to the
  // wildcard source pattern matching the destination.
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", true, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       CrossOriginNavigationInStaticBlockListAndAllowList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // The navigation should be allowed because the allow list is checked before
  // the block list.
  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       StaticBlockOverridesDynamicList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]example.com", "to": "[*.]foo.com" }
      ]
    }
  )json");

  OpenGlicAndCreateTask();
  actor_task().GetExecutionEngine()->AddWritableMainframeOrigins(
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
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);

  actor_keyed_service().ResetForTesting();

  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", true, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       StaticAllowListOverridesDynamicList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL allowed_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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

  histogram_tester_for_init_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.CrossSite", true, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigationBlockedByStaticList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // First navigation should be allowed due to same origin.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigationWithOpaqueSourceOriginBlockedUnderWildcard) {
  const GURL blocked_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigateToSandboxedPageBlockedByStaticList) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sandboxed_url = embedded_https_test_server().GetURL(
      "foo.com", "/actor/sandbox_main_frame_csp.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // First navigation should be allowed due to same origin.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingBrowserTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "MultiInstance"
                                             : "SingleInstance";
                         });

class ExecutionEngineOriginGatingParamBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ExecutionEngineOriginGatingParamBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kGlicCrossOriginNavigationGating,
             {{
                 {"prompt_user_for_sensitive_navigations",
                  prompt_user_for_sensitive_navigations_enabled() ? "true"
                                                                  : "false"},
                 {"confirm_navigation_to_new_origins",
                  confirm_navigation_to_new_origins_enabled() ? "true"
                                                              : "false"},
                 {"prompt_user_for_navigation_to_new_origins",
                  prompt_user_for_navigation_to_new_origins_enabled()
                      ? "true"
                      : "false"},
             }}},
        },
        /*disabled_features=*/{});
  }
  ~ExecutionEngineOriginGatingParamBrowserTest() override = default;

  bool prompt_user_for_sensitive_navigations_enabled() {
    return std::get<0>(GetParam());
  }
  bool confirm_navigation_to_new_origins_enabled() {
    return std::get<1>(GetParam());
  }
  bool prompt_user_for_navigation_to_new_origins_enabled() {
    return std::get<2>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       ConfirmBlockedOriginWithUserDisabled) {
  if (prompt_user_for_sensitive_navigations_enabled()) {
    GTEST_SKIP() << "prompt_user_for_sensitive_navigations enabled already "
                    "tested in ExecutionEngineOriginGatingBrowserTest.";
  }

  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  OpenGlicAndCreateTask();
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", blocked_url)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       NavigationConfirmationDisabled) {
  if (confirm_navigation_to_new_origins_enabled()) {
    GTEST_SKIP() << "confirm_navigation_to_new_origins enabled already tested "
                    "in ExecutionEngineOriginGatingBrowserTest.";
    ;
  }

  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Have navigation confirmations reject new origins. This should not stop the
  // navigation when confirm_navigation_to_new_origins is disabled.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       PromptUserForNewOrigin) {
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

  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(second_url).GetDebugString())
          .Set("forBlocklistedOrigin", false);
  VerifyUserConfirmationDialogRequest(expected_request);

  // Trigger ExecutionEngine destructor for metrics.
  actor_keyed_service().ResetForTesting();

  // Should add the origin to the allowlist.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AllowListSize", 1, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       ConfirmWithUserForMayActOnTab) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  OpenGlicAndCreateTask();

  // Mock IPC response will always confirm the request.
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  // Start on blocked.example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Clicks on full-page link to bar.com.
  std::unique_ptr<ToolRequest> click_link =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));

  ActResultFuture result;
  actor_task().Act(ToRequestList(click_link), result.GetCallback());
  if (prompt_user_for_sensitive_navigations_enabled()) {
    ExpectOkResult(result);
  } else {
    ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);
  }

  // Trigger ExecutionEngine destructor for metrics.
  actor_keyed_service().ResetForTesting();

  // If prompting is enabled, there should be a single confirmation.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true,
      prompt_user_for_sensitive_navigations_enabled() ? 1 : 0);
  // If prompting is enabled, the allow-list should have 1 entry at the end of
  // the task.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AllowListSize", 1,
      prompt_user_for_sensitive_navigations_enabled() ? 1 : 0);
}

// Tuple values are:
// (prompt_user_for_sensitive_navigations,
//  confirm_navigation_to_new_origins,
//  prompt_user_for_navigation_to_new_origins).
INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingParamBrowserTest,
                         testing::Values(std::make_tuple(false, true, false),
                                         std::make_tuple(true, false, false),
                                         std::make_tuple(true, true, true)),
                         [](auto& info) {
                           if (!std::get<0>(info.param)) {
                             return "UserConfirmDisabled";
                           }
                           if (!std::get<1>(info.param)) {
                             return "NavigationConfirmDisabled";
                           }
                           if (std::get<2>(info.param)) {
                             return "PromptToConfirmNavigation";
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
                       IgnoreBlocklist) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocked_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // Create a navigation request to the blocked URL.
  std::unique_ptr<ToolRequest> navigate_to_blocked =
      MakeNavigateRequest(*active_tab(), blocked_url.spec());

  // Execute the navigation action.
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_blocked), result.GetCallback());

  // The navigation should succeed because the safety checks are disabled.
  ExpectOkResult(result);

  // Verify that the browser navigated to the blocked URL.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), blocked_url);
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
                       ConfirmNavigationToNewSite_Denied) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL same_site = embedded_https_test_server().GetURL(
      "other.example.com", "/actor/link.html");
  const GURL cross_site =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  // Same origin should never trigger gating
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  // Cross origin but same site should only trigger when we're gating on origin
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", same_site)));
  ClickTarget("#link",
              should_gate_by_site()
                  ? mojom::ActionResultCode::kOk
                  : mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // Cross site will always trigger gating
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", cross_site)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  // Should log that permission was *denied* once.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", false,
      should_gate_by_site() ? 1 : 2);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest,
                       ConfirmListAlwaysUsesOrigin) {
  if (!should_gate_by_site()) {
    GTEST_SKIP() << "Confirmlist already tested in "
                    "ExecutionEngineOriginGatingBrowserTest.";
  }
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL confirmlist_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, false)));

  ASSERT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", confirmlist_url)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  auto expected_request =
      base::Value::Dict()
          .Set("navigationOrigin",
               url::Origin::Create(confirmlist_url).GetDebugString())
          .Set("forBlocklistedOrigin", true);
  VerifyUserConfirmationDialogRequest(expected_request);

  // Should log that permission was *denied* once.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", false, 1);
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
      "bar.com", base::StrCat({"/actor/link_full_page.html?href=",
                               EncodeURI(other_url_same_site.spec())}));

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

}  // namespace actor
