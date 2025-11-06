// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
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
          resolve({
            navigationOrigin: request.navigationOrigin,
          });
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
              resolve({
                navigationOrigin: request.navigationOrigin,
              });
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor,
                              kGlicCrossOriginNavigationGating},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ~ExecutionEngineOriginGatingBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");

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
  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", url::Origin::Create(second_url).GetDebugString());
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
  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", url::Origin::Create(second_url).GetDebugString());
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
  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", url::Origin::Create(blocked_url).GetDebugString());
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
  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", url::Origin::Create(blocked_url).GetDebugString());
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
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  const GURL blocked_origin_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");

  OpenGlicAndCreateTask();

  // Start on example.com.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  // Navigate to blocked
  std::unique_ptr<ToolRequest> navigate_to_blocked =
      MakeNavigateRequest(*active_tab(), blocked_origin_url.spec());
  // Navigate from back to start
  std::unique_ptr<ToolRequest> navigate_back_to_start =
      MakeNavigateRequest(*active_tab(), start_url.spec());
  // Navigate from back to blocked
  std::unique_ptr<ToolRequest> navigate_back_to_blocked =
      MakeNavigateRequest(*active_tab(), blocked_origin_url.spec());

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));
  ActResultFuture result;
  actor_task().Act(ToRequestList(navigate_to_blocked, navigate_back_to_start,
                                 navigate_back_to_blocked),
                   result.GetCallback());
  ExpectOkResult(result);

  VerifyUserConfirmationDialogRequest(base::Value::Dict().Set(
      "navigationOrigin",
      url::Origin::Create(blocked_origin_url).GetDebugString()));

  // Trigger ExecutionEngine destructor for metrics.
  actor_keyed_service().ResetForTesting();

  // We should have applied the gate twice. Once for each navigation to blocked.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", false, 1);
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AppliedGate", true, 2);
  // Permission should have been explicitly granted twice. Once for each
  // navigation to blocked.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true, 2);
  // The allow-list should have 2 entries at the end of the task.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AllowListSize", 2, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
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
  ExpectOkResult(result);

  actor_keyed_service().ResetForTesting();

  // Should log that permission was denied the one time it was prompted.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true, 1);
  // The allow-list should have 1 entry at the end of the task.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AllowListSize", 1, 1);
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

  // Trigger ExecutionEngine destructor for metrics.
  actor_keyed_service().ResetForTesting();

  // Should add the origin to the allowlist.
  histogram_tester_for_init_.ExpectBucketCount(
      "Actor.NavigationGating.AllowListSize", 1, 1);
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

}  // namespace actor
