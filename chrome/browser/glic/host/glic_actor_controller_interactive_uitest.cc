// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace glic::test {

namespace {

using ::optimization_guide::proto::AnnotatedPageContent;
using ::optimization_guide::proto::BrowserAction;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::ContentAttributes;
using ::optimization_guide::proto::ContentNode;

std::string EncodeActionProto(const BrowserAction& action) {
  return base::Base64Encode(action.SerializeAsString());
}

class GlicActorControllerUiTest : public test::InteractiveGlicTest {
 public:
  using ActionProtoProvider = base::OnceCallback<std::string()>;

  GlicActorControllerUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActor, optimization_guide::features::
                                   kAnnotatedPageContentWithActionableElements},
        {});
  }
  ~GlicActorControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();

    // TODO(crbug.com/409564704): Mock the delay so that tests can run at
    // reasonable speed. Remove once there is a more permanent approach.
    actor::OverrideActionObservationDelay(base::Milliseconds(10));
  }

  // Executes a BrowserAction and verifies it succeeds. Optionally takes an
  // error reason which, when provided, causes failure if the action is
  // successful or fails with an unexpected reason.
  //
  // The action is passed as a proto "provider" which is a callback that returns
  // a string which is the base-64 representation of the BrowserAction proto to
  // invoke. This is a callback rather than a BrowserAction since, in some
  // cases, the parameters in the proto may depend on prior test steps (such as
  // extracting the AnnotatedPageContent, so that the provider can then find the
  // content node id from the APC). See the Provider methods below (e.g.
  // ClickActionProvider).
  auto ExecuteAction(ActionProtoProvider proto_provider,
                     base::Value::Dict context_options,
                     std::optional<glic::mojom::ActInFocusedTabErrorReason>
                         expected_error = std::nullopt) {
    constexpr int kResultSuccess = -1;
    constexpr std::string kSuccessString = "<Success>";
    CHECK_LT(kResultSuccess,
             static_cast<int>(mojom::ActInFocusedTabErrorReason::kMinValue));

    auto result_buffer = std::make_unique<std::optional<int>>();
    std::optional<int>* buffer_raw = result_buffer.get();
    return Steps(
        InAnyContext(WithElement(
            kGlicContentsElementId,
            [result_out = buffer_raw,
             proto_provider = std::move(proto_provider),
             context_options = std::move(context_options),
             kResultSuccess](ui::TrackedElement* el) mutable {
              content::WebContents* glic_contents =
                  AsInstrumentedWebContents(el)->web_contents();
              std::string script = content::JsReplace(
                  R"js(
                              (async () => {
                                const base64ToArrayBuffer = (base64) => {
                                  const bytes = window.atob(base64);
                                  const len = bytes.length;
                                  const ret = new Uint8Array(len);
                                  for (var i = 0; i < len; i++) {
                                    ret[i] = bytes.charCodeAt(i);
                                  }
                                  return ret.buffer;
                                }
                                try {
                                  await client.browser.actInFocusedTab({
                                    actionProto: base64ToArrayBuffer($1),
                                    tabContextOptions: $2
                                  });
                                  // Return success.
                                  return $3;
                                } catch (err) {
                                  return err.reason;
                                }
                              })();
                            )js",
                  std::move(proto_provider).Run(), std::move(context_options),
                  kResultSuccess);
              *result_out = content::EvalJs(glic_contents, std::move(script))
                                .ExtractInt();
            })),
        CheckResult(
            [result_in = std::move(result_buffer), &kSuccessString]() {
              CHECK(result_in->has_value());

              int result = result_in->value();
              if (result == kResultSuccess) {
                return kSuccessString;
              }
              auto result_enum =
                  static_cast<mojom::ActInFocusedTabErrorReason>(result);
              EXPECT_TRUE(mojom::IsKnownEnumValue(result_enum));
              return base::ToString(result_enum);
            },
            expected_error.has_value() ? base::ToString(expected_error.value())
                                       : kSuccessString,
            "ExecuteAction"));
  }

  // Overload of the above method that allows passing a BrowserAction directly,
  // if one can be constructed ahead of time i.e. does not depend on an
  // observation.
  auto ExecuteAction(const BrowserAction& action,
                     base::Value::Dict context_options,
                     std::optional<glic::mojom::ActInFocusedTabErrorReason>
                         expected_error = std::nullopt) {
    return ExecuteAction(PassthroughProvider(action),
                         std::move(context_options), expected_error);
  }
  // Starts a new task by executing an initial navigate action to `task_url` to
  // create a new tab. The new tab can then be referenced by the identifier
  // passed in `new_tab_id`.
  auto StartActorTaskInNewTab(const GURL& task_url,
                              ui::ElementIdentifier new_tab_id) {
    BrowserAction start_navigate = actor::MakeNavigate(task_url.spec());

    return Steps(InstrumentNextTab(new_tab_id),
                 ExecuteAction(start_navigate, AnnotationsOnlyContextOptions()),
                 WaitForWebContentsReady(new_tab_id, task_url));
  }

  // After invoking APIs that don't return promises, we round trip to both the
  // client and host to make sure the call has made it to the browser.
  auto RoundTrip() {
    return Steps(InAnyContext(WithElement(
                     kGlicContentsElementId,
                     [](ui::TrackedElement* el) {
                       content::WebContents* glic_contents =
                           AsInstrumentedWebContents(el)->web_contents();
                       ASSERT_TRUE(content::ExecJs(glic_contents, "true;"));
                     })),
                 InAnyContext(WithElement(
                     kGlicHostElementId, [](ui::TrackedElement* el) {
                       content::WebContents* webui_contents =
                           AsInstrumentedWebContents(el)->web_contents();
                       ASSERT_TRUE(content::ExecJs(webui_contents, "true;"));
                     })));
  }

  // Stops a running task by calling the glic StopActorTask API.
  auto StopActorTask() {
    return Steps(InAnyContext(WithElement(
                     kGlicContentsElementId,
                     [](ui::TrackedElement* el) {
                       content::WebContents* glic_contents =
                           AsInstrumentedWebContents(el)->web_contents();
                       constexpr std::string_view script =
                           "client.browser.stopActorTask(0);";
                       ASSERT_TRUE(content::ExecJs(glic_contents, script));
                     })),
                 RoundTrip());
  }

  // Pauses a running task by calling the glic PauseActorTask API.
  auto PauseActorTask() {
    return Steps(InAnyContext(WithElement(
                     kGlicContentsElementId,
                     [](ui::TrackedElement* el) {
                       content::WebContents* glic_contents =
                           AsInstrumentedWebContents(el)->web_contents();
                       constexpr std::string_view script =
                           "client.browser.pauseActorTask(0);";
                       ASSERT_TRUE(content::ExecJs(glic_contents, script));
                     })),
                 RoundTrip());
  }

  // Resumes a paused task by calling the glic ResumeActorTask API.
  auto ResumeActorTask(base::Value::Dict context_options, bool expected) {
    return Steps(InAnyContext(CheckElement(
        kGlicContentsElementId,
        [context_options =
             std::move(context_options)](ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          std::string script = content::JsReplace(
              R"js(
                              (async () => {
                                try {
                                  await client.browser.resumeActorTask(0, $1);
                                  return true;
                                } catch (err) {
                                  return false;
                                }
                              })();
                            )js",
              std::move(context_options));
          return content::EvalJs(glic_contents, script).ExtractBool();
        },
        expected)));
  }

  // Returns a callback that builds an encoded proto for a click action on a
  // ContentNode that matches the passed in label.
  // Note: This currently assumes acting is occurring on the focused tab.
  ActionProtoProvider ClickActionProvider(std::string_view label) {
    return base::BindLambdaForTesting([this, label]() {
      content::RenderFrameHost* rfh = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
      int32_t node_id = this->SearchAnnotatedPageContent(label);
      return EncodeActionProto(actor::MakeClick(*rfh, node_id));
    });
  }

  // Returns a callback that builds an encoded proto for a click action on a
  // specific dom_node_id.
  // Note: This currently assumes acting is occurring on the focused tab.
  ActionProtoProvider ClickActionProvider(int32_t node_id) {
    return base::BindLambdaForTesting([this, node_id]() {
      content::RenderFrameHost* rfh = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
      return EncodeActionProto(actor::MakeClick(*rfh, node_id));
    });
  }

  // Returns a callback that simply encodes the given action.
  ActionProtoProvider PassthroughProvider(const BrowserAction& action) {
    return base::BindLambdaForTesting(
        [action]() { return EncodeActionProto(action); });
  }

  // Returns a callback that returns the given string as the action proto. Meant
  // for testing error handling since this allows providing an invalid proto.
  ActionProtoProvider ArbitraryStringProvider(std::string_view str) {
    return base::BindLambdaForTesting([str]() { return std::string(str); });
  }

  // Gets the context options to capture a new observation after completing an
  // action. This includes both annotations (i.e. AnnotatedPageContent) and a
  // screenshot.
  base::Value::Dict UpdatedContextOptions() {
    return base::Value::Dict()
        .Set("annotatedPageContent", true)
#if BUILDFLAG(IS_LINUX)
        // TODO(https://crbug.com/40191775): Tests on Linux aren't producing
        // graphical output so requesting a screenshot hangs forever.
        .Set("viewportScreenshot", false);
#else
        .Set("viewportScreenshot", true);
#endif
  }

  // Gets the context options to capture a new observation that only has the
  // annotations (AnnotatedPageContent). Taking screenshots can be slow or flaky
  // in the test. This is intended to be used for interim steps, *before*
  // returning the final context of an action.
  base::Value::Dict AnnotationsOnlyContextOptions() {
    return base::Value::Dict()
        .Set("annotatedPageContent", true)
        .Set("viewportScreenshot", false);
  }

  auto InitializeWithOpenGlicWindow() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCurrentActiveTabId);

    // Navigate to ensure the initial tab has some valid content loaded that the
    // Glic window can observe.
    const GURL start_url =
        embedded_test_server()->GetURL("/actor/blank.html?start");

    return Steps(InstrumentTab(kCurrentActiveTabId),
                 NavigateWebContents(kCurrentActiveTabId, start_url),
                 OpenGlicWindow(GlicWindowMode::kAttached));
  }

  // Retrieves AnnotatedPageContent for the currently focused tab (and caches
  // it in `annotated_page_content_`).
  auto GetPageContextFromFocusedTab() {
    return Steps(Do([&]() {
      GlicKeyedService* glic_service =
          GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
      ASSERT_TRUE(glic_service);

      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

      auto options = mojom::GetTabContextOptions::New();
      options->include_annotated_page_content = true;

      FetchPageContext(
          glic_service->GetFocusedTabData(), *options,
          base::BindLambdaForTesting([&](mojom::GetContextResultPtr result) {
            mojo_base::ProtoWrapper& serialized_apc =
                *result->get_tab_context()
                     ->annotated_page_data->annotated_page_content;
            annotated_page_content_ = std::make_unique<AnnotatedPageContent>(
                serialized_apc.As<AnnotatedPageContent>().value());
            run_loop.Quit();
          }));

      run_loop.Run();
    }));
  }

  auto CheckHasTaskForTab(ui::ElementIdentifier tab, bool expected) {
    return Steps(InAnyContext(CheckElement(
        tab,
        [](ui::TrackedElement* el) {
          content::WebContents* tab_contents =
              AsInstrumentedWebContents(el)->web_contents();
          const auto* glic_service =
              GlicKeyedService::Get(tab_contents->GetBrowserContext());
          return glic_service &&
                 glic_service->IsActorCoordinatorActingOnTab(tab_contents);
        },
        expected)));
  }

 private:
  int32_t SearchAnnotatedPageContent(std::string_view label) {
    CHECK(annotated_page_content_)
        << "An observation must be made with GetPageContextFromFocusedTab "
           "before searching annotated page content.";

    // Traverse the APC in depth-first preorder, returning the first node that
    // matches the given label.
    std::stack<const ContentNode*> nodes;
    nodes.push(&annotated_page_content_->root_node());

    while (!nodes.empty()) {
      const ContentNode* current = nodes.top();
      nodes.pop();

      if (current->content_attributes().label() == label) {
        return current->content_attributes().common_ancestor_dom_node_id();
      }

      for (const auto& child : current->children_nodes()) {
        nodes.push(&child);
      }
    }

    // Tests must pass a label that matches one of the content nodes.
    NOTREACHED() << "Label [" << label << "] not found in page.";
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AnnotatedPageContent> annotated_page_content_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, OpensNewTabOnFirstNavigate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  BrowserAction navigate = actor::MakeNavigate(task_url.spec());

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  InstrumentNextTab(kNewActorTabId),
                  ExecuteAction(navigate, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, task_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       UsesExistingActorTabOnSubsequentNavigate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const GURL second_navigate_url =
      embedded_test_server()->GetURL("/actor/blank.html?second");
  BrowserAction second_navigate =
      actor::MakeNavigate(second_navigate_url.spec());

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  // Now that the task is started in a new tab, do the
                  // second navigation.
                  ExecuteAction(second_navigate, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, second_navigate_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                                UpdatedContextOptions()),
                  WaitForJsResult(kNewActorTabId, "() => button_clicked"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionProtoInvalid) {
  std::string encodedProto = base::Base64Encode("invalid serialized bytes");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      ExecuteAction(
          ArbitraryStringProvider(encodedProto), UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kInvalidActionProto));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionTargetNotFound) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr int32_t kNonExistentContentNodeId =
      std::numeric_limits<int32_t>::max();
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      ExecuteAction(ClickActionProvider(kNonExistentContentNodeId),
                    UpdatedContextOptions(),
                    glic::mojom::ActInFocusedTabErrorReason::kTargetNotFound));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, HistoryTool) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url_1 = embedded_test_server()->GetURL("/actor/blank.html?1");
  const GURL url_2 = embedded_test_server()->GetURL("/actor/blank.html?2");
  BrowserAction navigate_url_2 = actor::MakeNavigate(url_2.spec());
  BrowserAction back = actor::MakeHistoryBack();
  BrowserAction forward = actor::MakeHistoryForward();

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(url_1, kNewActorTabId),
                  ExecuteAction(navigate_url_2, UpdatedContextOptions()),
                  ExecuteAction(back, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, url_1),
                  ExecuteAction(forward, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, url_2));
}

// Ensure that a task can be stopped and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, StopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions()),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckHasTaskForTab(kNewActorTabId, true), StopActorTask(),
      // TODO(crbug.com/409558980): Expect kTargetNotFound since that's
      // currently the error returned anytime a tool fails but in the future we
      // should add an error code for "NoActiveTask".
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions(),
                    glic::mojom::ActInFocusedTabErrorReason::kTargetNotFound),
      CheckHasTaskForTab(kNewActorTabId, false));
}

// Ensure that a task can be started after a previous task was stopped.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, StopThenStartActTask) {
  constexpr std::string_view kClickableButtonLabel = "clickable";
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      // Start and stop.
      StartActorTaskInNewTab(task_url, kFirstTabId), StopActorTask(),

      // Start, click, stop.
      StartActorTaskInNewTab(task_url, kSecondTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions()),
      WaitForJsResult(kSecondTabId, "() => button_clicked"), StopActorTask(),

      // Start, click, stop.
      StartActorTaskInNewTab(task_url, kThirdTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions()),
      WaitForJsResult(kThirdTabId, "() => button_clicked"), StopActorTask());
}

// Ensure that a task can be paused and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions()),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckHasTaskForTab(kNewActorTabId, true), PauseActorTask(),
      // TODO(crbug.com/409558980): Expect kTargetNotFound since that's
      // currently the error returned anytime a tool fails but in the future we
      // should add an error code for "NoActiveTask".
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions(),
                    glic::mojom::ActInFocusedTabErrorReason::kTargetNotFound),
      // Unlike stopping, pausing keeps the task.
      CheckHasTaskForTab(kNewActorTabId, true));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseThenStopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                                UpdatedContextOptions()),
                  WaitForJsResult(kNewActorTabId, "() => button_clicked"),
                  PauseActorTask(), CheckHasTaskForTab(kNewActorTabId, true),
                  StopActorTask(), CheckHasTaskForTab(kNewActorTabId, false));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseAlreadyPausedActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                                UpdatedContextOptions()),
                  WaitForJsResult(kNewActorTabId, "() => button_clicked"),
                  PauseActorTask(), PauseActorTask(),
                  CheckHasTaskForTab(kNewActorTabId, true));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseThenResumeActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions()),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      ExecuteJs(kNewActorTabId, "() => { button_clicked = false; }"),
      PauseActorTask(), ResumeActorTask(UpdatedContextOptions(), true),
      CheckHasTaskForTab(kNewActorTabId, true),
      ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                    UpdatedContextOptions()),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ResumeActorTaskWithoutATask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  StopActorTask(), CheckHasTaskForTab(kNewActorTabId, false),
                  // Once a task is stopped, it can't be resumed.
                  ResumeActorTask(UpdatedContextOptions(), false));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ResumeActorTaskWhenAlreadyResumed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  PauseActorTask(),
                  ResumeActorTask(UpdatedContextOptions(), true),
                  ResumeActorTask(UpdatedContextOptions(), false));
}

class GlicActorControllerWithActorDisabledUiTest
    : public test::InteractiveGlicTest {
 public:
  GlicActorControllerWithActorDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicActor);
  }
  ~GlicActorControllerWithActorDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerWithActorDisabledUiTest,
                       ActorNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.actInFocusedTab); }")));
}

}  //  namespace

}  // namespace glic::test
