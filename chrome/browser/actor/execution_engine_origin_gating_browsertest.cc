// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/actor/actor_features.h"
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
#include "components/optimization_guide/core/filters/hints_component_util.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
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
              subscription.unsubscribe();
            }
          );
    });
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

class ExecutionEngineOriginGatingBrowserTestBase
    : public glic::NonInteractiveGlicTest {
 public:
  ExecutionEngineOriginGatingBrowserTestBase() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic, {}},
            {features::kGlicActor,
             {{features::kGlicActorPolicyControlExemption.name, "true"}}},
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

    // Optimization guide uses this histogram to signal initialization in tests.
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath proto_path =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("base_proto.pb"));
    ASSERT_TRUE(SetUpOptimizationGuideComponentBlocklist(
        proto_path, "blocked.example.com"));
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent({base::Version("1"), proto_path});

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  virtual bool multi_instance_enabled() {
    return base::FeatureList::IsEnabled(features::kGlicMultiInstance);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  [[nodiscard]] InteractiveTestApi::MultiStep CreateMockWebClientRequest(
      const std::string_view handle_dialog_js,
      const base::Location& location = FROM_HERE) {
    return InAnyContext(WithElement(
        glic::test::kGlicContentsElementId,
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
        glic::test::kGlicContentsElementId,
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
  base::ScopedTempDir temp_dir_;

 private:
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TaskId task_id_;
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
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
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
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmBlockedOriginWithUser_Granted) {
  base::HistogramTester histogram_tester;
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
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(blocked_url)))));

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
IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingUserPromptingBrowserTest,
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
  RunTestSequence(VerifyUserConfirmationDialogRequest(base::test::ParseJsonDict(
      content::JsReplace(R"({
    "navigationOrigin": $1,
    "forBlocklistedOrigin": false
  })",
                         url::Origin::Create(start_url)))));

  // Now this should proceed without a user confirmation or a server
  // confirmation, since the user has already confirmed it.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), other_url));
}

// When kGlicPromptUserForNavigationToNewOrigins is enabled, we should not
// prompt twice even if the origin becomes sensitive during the task.
IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingUserPromptingBrowserTest,
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
  RunTestSequence(VerifyUserConfirmationDialogRequest(base::test::ParseJsonDict(
      content::JsReplace(R"({
    "navigationOrigin": $1,
    "forBlocklistedOrigin": false
  })",
                         url::Origin::Create(start_url)))));

  // Now this should proceed without a user confirmation or a server
  // confirmation, since the user has already confirmed it.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), eventually_sensitive));
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       ConfirmBlockedOriginWithUser_Denied) {
  base::HistogramTester histogram_tester;
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
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(blocked_url)))));

  // Should log that permission was *denied* once.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", false, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
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

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
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
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       BlockedNavigationNotAddedToAllowlist) {
  base::HistogramTester histogram_tester;
  const GURL start_url = embedded_https_test_server().GetURL(
      "www.example.com", "/actor/blank.html");
  const GURL blocked_origin_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  const GURL blocked_origin_link_url = embedded_https_test_server().GetURL(
      "blocked.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(blocked_origin_url.spec())}));
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "www.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(blocked_origin_url.spec())}));

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

  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(blocked_origin_url)))));

  // Trigger ExecutionEngine destructor for metrics.
  StopAllTasks();

  // Navigation gating should only be applied to the first navigation action.
  // All other navigations should not have gating.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Actor.NavigationGating.AppliedGate"),
      base::BucketsAre(base::Bucket(false, 3), base::Bucket(true, 1)));
  // Permission should have been explicitly granted twice. Once for each
  // navigation to blocked.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     true, 1);
  // The allow-list should have 2 entries at the end of the task.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AllowListSize", 2,
                                     1);
  // The list of confirmed sensitive origins should have 1 entry.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.ConfirmedListSize2", 1, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       SandboxedSiteDoesNotReprompt) {
  base::HistogramTester histogram_tester;
  const GURL sandboxed_blocked_page = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/sandboxed_blank.html");
  const GURL blocked_page = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  const GURL normal_page_with_link = embedded_https_test_server().GetURL(
      "www.example.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(blocked_page.spec())}));

  // Start on sandboxed page.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), sandboxed_blocked_page));
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
  // confirmed when during MayActOnTab.
  histogram_tester.ExpectUniqueSample("Actor.NavigationGating.AppliedGate",
                                      false, 2);
  // Permission should have been explicitly granted once during MayActOnTab. The
  // navigation to to `www.example.com` had implicit permission via the tool
  // request.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.PermissionGranted", true, 1);
  // The allow-list should have 2 entries at the end of the task.
  histogram_tester.ExpectUniqueSample("Actor.NavigationGating.AllowListSize", 2,
                                      1);
  // The list of confirmed sensitive origins should have 1 entry.
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.ConfirmedListSize2", 1, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigationNotGatedWithStaticList) {
  base::HistogramTester histogram_tester;
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
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       SameOriginNavigationInStaticAllowList) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       CrossOriginNavigationInStaticBlockListAndAllowList) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       StaticBlockOverridesDynamicList) {
  base::HistogramTester histogram_tester;
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
                    mojom::ActionResultCode::kTriggeredNavigationBlocked);

  StopAllTasks();

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AppliedGate", true,
                                     1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       StaticAllowListOverridesDynamicList) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowByStaticList, 1);
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AppliedGate",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(kSameOriginSourceHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteSourceHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameOriginInitiatorHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kSameSiteInitiatorHistogram, false, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigationBlockedByStaticList) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigationWithOpaqueSourceOriginBlockedUnderWildcard) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       NavigateToSandboxedPageBlockedByStaticList) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kAllowSameOrigin, 1);
  // Second navigation should be blocked by static blocklist = 3.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.GatingDecision",
      ExecutionEngine::GatingDecision::kBlockByStaticList, 1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingBrowserTest,
                       BlocklistAppliesToMayActOnTab) {
  const GURL start_url = embedded_https_test_server().GetURL(
      "bad.example.com", "/actor/link.html");
  SafetyListManager::GetInstance()->ParseSafetyLists(R"json(
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
  const auto expected_result =
      base::FeatureList::IsEnabled(kGlicGranularBlockingActionResultCodes)
          ? mojom::ActionResultCode::kActionsBlockedForSiteRisk
          : mojom::ActionResultCode::kUrlBlocked;
  ExpectErrorResult(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingBrowserTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "MultiInstance"
                                             : "SingleInstance";
                         });
INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineOriginGatingUserPromptingBrowserTest,
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

  // Should add the origin to the allowlist.
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.AllowListSize", 1,
                                     1);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineOriginGatingParamBrowserTest,
                       ConfirmWithUserForMayActOnTab) {
  base::HistogramTester histogram_tester;
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
  StopAllTasks();

  // If prompting is enabled, there should be a single confirmation.
  histogram_tester.ExpectBucketCount(
      "Actor.NavigationGating.PermissionGranted", true,
      prompt_user_for_sensitive_navigations_enabled() ? 1 : 0);
  // If prompting is enabled, the allow-list should have 1 entry at the end of
  // the task.
  histogram_tester.ExpectBucketCount(
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
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectBucketCount("Actor.NavigationGating.PermissionGranted",
                                     false, should_gate_by_site() ? 1 : 2);
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineSiteGatingBrowserTest,
                       ConfirmListAlwaysUsesOrigin) {
  base::HistogramTester histogram_tester;
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
  RunTestSequence(VerifyUserConfirmationDialogRequest(
      base::test::ParseJsonDict(content::JsReplace(
          R"({"navigationOrigin": $1, "forBlocklistedOrigin": true})",
          url::Origin::Create(confirmlist_url)))));

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

class ExecutionEngineGatingConfirmationMetricBrowserTest
    : public ExecutionEngineOriginGatingBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineGatingConfirmationMetricBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {kGlicCrossOriginNavigationGating,
         {{"confirm_navigation_to_new_origins", "false"}}}};
    std::vector<base::test::FeatureRef> disabled_features;

    if (recording_metrics_enabled()) {
      enabled_features.push_back(
          {kGlicRecordNavigationConfirmationRequestMetrics, {}});
    } else {
      disabled_features.push_back(
          kGlicRecordNavigationConfirmationRequestMetrics);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  ~ExecutionEngineGatingConfirmationMetricBrowserTest() override = default;

  bool recording_metrics_enabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExecutionEngineGatingConfirmationMetricBrowserTest,
                       SameOriginNavigation) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  StopAllTasks();
  if (recording_metrics_enabled()) {
    base::test::RunUntil([&]() {
      return histogram_tester
                 .GetAllSamples(
                     "Actor.NavigationGating.ActionNavigationsApprovedByServer")
                 .size() == 1;
    });
    histogram_tester.ExpectUniqueSample(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", true, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineGatingConfirmationMetricBrowserTest,
                       NovelNavigation) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL novel_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", novel_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  StopAllTasks();
  if (recording_metrics_enabled()) {
    base::test::RunUntil([&]() {
      return histogram_tester
                 .GetAllSamples(
                     "Actor.NavigationGating.ActionNavigationsApprovedByServer")
                 .size() == 1;
    });
    histogram_tester.ExpectUniqueSample(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", true, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineGatingConfirmationMetricBrowserTest,
                       NovelNavigation_denied) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL novel_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", novel_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  StopAllTasks();
  if (recording_metrics_enabled()) {
    base::test::RunUntil([&]() {
      return histogram_tester
                 .GetAllSamples(
                     "Actor.NavigationGating.ActionNavigationsApprovedByServer")
                 .size() == 1;
    });
    histogram_tester.ExpectUniqueSample(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", false, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineGatingConfirmationMetricBrowserTest,
                       SensitiveNavigation) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL sensitive_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  // We need to set up both the user AND navigation confirmation responses since
  // the user confirmation is used for sensitive sites
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));
  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleUserConfirmationDialogTempl, true)));

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", sensitive_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  StopAllTasks();
  if (recording_metrics_enabled()) {
    base::test::RunUntil([&]() {
      return histogram_tester
                 .GetAllSamples(
                     "Actor.NavigationGating.ActionNavigationsApprovedByServer")
                 .size() == 1;
    });
    histogram_tester.ExpectUniqueSample(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", true, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineGatingConfirmationMetricBrowserTest,
                       AllowlistedNavigation) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL allowlisted_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(
      content::JsReplace(R"json(
    {
      "navigation_allowed": [
        { "from": "*", "to": $1 }
      ]
    }
  )json",
                         allowlisted_url.host()));

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", allowlisted_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  StopAllTasks();
  if (recording_metrics_enabled()) {
    base::test::RunUntil([&]() {
      return histogram_tester
                 .GetAllSamples(
                     "Actor.NavigationGating.ActionNavigationsApprovedByServer")
                 .size() == 1;
    });
    histogram_tester.ExpectUniqueSample(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", true, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Actor.NavigationGating.ActionNavigationsApprovedByServer", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ExecutionEngineGatingConfirmationMetricBrowserTest,
                       BlocklistedNavigation) {
  base::HistogramTester histogram_tester;
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL blocklisted_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  SafetyListManager::GetInstance()->ParseSafetyLists(
      content::JsReplace(R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": $1 }
      ]
    }
  )json",
                         blocklisted_url.host()));

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  OpenGlicAndCreateTask();

  RunTestSequence(CreateMockWebClientRequest(
      content::JsReplace(kHandleNavigationConfirmationTempl, true)));

  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", blocklisted_url)));
  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);

  StopAllTasks();

  histogram_tester.ExpectTotalCount(
      "Actor.NavigationGating.ActionNavigationsApprovedByServer", 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionEngineGatingConfirmationMetricBrowserTest,
                         testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "MetricsEnabled"
                                             : "MetricsDisabled";
                         });

}  // namespace actor
