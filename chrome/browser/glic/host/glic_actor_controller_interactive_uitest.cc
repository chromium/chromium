// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/ozone/public/ozone_platform.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

using ::actor::TaskId;
using apc::Actions;
using apc::ActionsResult;
using apc::AnnotatedPageContent;
using apc::ClickAction;
using ClickType = apc::ClickAction::ClickType;
using ClickCount = apc::ClickAction::ClickCount;
using apc::ContentAttributes;
using apc::ContentNode;
using ::base::test::EqualsProto;
using ::content::RenderFrameHost;
using ::content::WebContents;
using ::tabs::TabHandle;
using ::tabs::TabInterface;

using HistoryDirection = ::actor::HistoryToolRequest::Direction;

constexpr char kActivateSurfaceIncompatibilityNotice[] =
    "Programmatic window activation does not work on the Weston reference "
    "implementation of Wayland used on Linux testbots. It also doesn't work "
    "reliably on Linux in general. For this reason, some of these tests which "
    "use ActivateSurface() may be skipped on machine configurations which do "
    "not reliably support them.";

std::string EncodeActionProto(const Actions& action) {
  return base::Base64Encode(action.SerializeAsString());
}

std::optional<ActionsResult> DecodeActionsResultProto(
    const std::string& base64_proto) {
  std::string decoded_proto;
  if (!base::Base64Decode(base64_proto, &decoded_proto)) {
    return std::nullopt;
  }
  optimization_guide::proto::ActionsResult actions_result;
  if (!actions_result.ParseFromString(decoded_proto)) {
    return std::nullopt;
  }
  return actions_result;
}

// Tests the actor framework using the Glic API surface. This tests is meant to
// exercise the API and end-to-end plumbing within Chrome. These tests aim to
// faithfully mimic Glic's usage of these APIs to provide some basic coverage
// that changes in Chrome aren't breaking Glic (though this relies on manual
// intervention anytime Glic changes and so is not a replacement for full
// end-to-end tests).
class GlicActorControllerUiTest : public test::InteractiveGlicTest {
 public:
  using ActionProtoProvider = base::OnceCallback<std::string()>;
  using ExpectedErrorResult = std::variant<std::monostate,
                                           actor::mojom::ActionResultCode,
                                           mojom::PerformActionsErrorReason>;
  static constexpr int32_t kNonExistentContentNodeId =
      std::numeric_limits<int32_t>::max();

  GlicActorControllerUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor,
                              features::kGlicActorToctouValidation,
                              optimization_guide::features::
                                  kAnnotatedPageContentWithActionableElements},
        /*disabled_features=*/{});
  }
  ~GlicActorControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    // Add rule for resolving cross origin host names.
    InteractiveGlicTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  const actor::ActorTask* GetActorTask() {
    auto* actor_service = actor::ActorKeyedService::Get(browser()->profile());
    return actor_service->GetTask(task_id_);
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
  // content node id from the APC). Prefer to use the wrappers like ClickAction,
  // NavigateAction, etc.
  auto ExecuteAction(ActionProtoProvider proto_provider,
                     ExpectedErrorResult expected_result = {}) {
    static constexpr int kResultSuccess =
        base::to_underlying(actor::mojom::ActionResultCode::kOk);
    static constexpr std::string_view kSuccessString = "<Success>";

    const std::string expected_result_string = std::visit(
        absl::Overload{
            [](std::monostate) { return std::string(kSuccessString); },
            [](actor::mojom::ActionResultCode r) {
              EXPECT_FALSE(actor::IsOk(r));
              return base::ToString(r);
            },
            [](mojom::PerformActionsErrorReason r) {
              return base::ToString(r);
            },
        },
        expected_result);

    auto result_buffer = std::make_unique<std::optional<int>>();
    std::optional<int>* buffer_raw = result_buffer.get();
    return Steps(
        InAnyContext(WithElement(
            kGlicContentsElementId,
            [result_out = buffer_raw,
             actions_result_out = &last_execution_result_,
             proto_provider =
                 std::move(proto_provider)](ui::TrackedElement* el) mutable {
              content::WebContents* glic_contents =
                  AsInstrumentedWebContents(el)->web_contents();
              // Distinguish errors from the action and errors from rejecting
              // performAction by making the latter negative.
              std::string script = content::JsReplace(
                  R"js(
                        (async () => {
                          try {
                            const res = await client.browser.performActions(
                              Uint8Array.fromBase64($1).buffer);
                            return new Uint8Array(res).toBase64();
                          } catch (err) {
                            return err.reason;
                          }
                        })();
                      )js",
                  std::move(proto_provider).Run());
              content::EvalJsResult result =
                  content::EvalJs(glic_contents, std::move(script));
              if (result.is_string()) {
                auto actions_result =
                    DecodeActionsResultProto(result.ExtractString());
                if (actions_result) {
                  *result_out = actions_result->action_result();
                  *actions_result_out = actions_result;
                } else {
                  *result_out = -static_cast<int>(
                      mojom::PerformActionsErrorReason::kInvalidProto);
                }
              } else {
                *result_out = -result.ExtractInt();
              }
            })),
        CheckResult(
            [result_in = std::move(result_buffer)]() {
              CHECK(result_in->has_value());

              int result = result_in->value();
              if (result == kResultSuccess) {
                return std::string(kSuccessString);
              }
              if (result < 0) {
                auto result_enum =
                    static_cast<mojom::PerformActionsErrorReason>(-result);
                EXPECT_TRUE(mojom::IsKnownEnumValue(result_enum));
                return base::ToString(result_enum);
              }
              auto result_enum =
                  static_cast<actor::mojom::ActionResultCode>(result);
              EXPECT_TRUE(actor::mojom::IsKnownEnumValue(result_enum));
              return base::ToString(result_enum);
            },
            expected_result_string, "ExecuteAction"));
  }

  auto CreateTask(actor::TaskId& out_task) {
    return InAnyContext(WithElement(
        kGlicContentsElementId, [&out_task](ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          const int result =
              content::EvalJs(glic_contents, "client.browser.createTask()")
                  .ExtractInt();
          out_task = actor::TaskId(result);
        }));
  }

  // Note: In all the Create*Action functions below, parameters that are
  // expected to be created as a result of test steps (task_id, tab_handle,
  // etc.) are passed by reference since they'll be evaluated at time of use
  // (i.e. when running the test step). Passing by non-const ref prevents
  // binding an rvalue argument to these parameters since the test step won't be
  // executed until after these functions are invoked.
  auto CreateTabAction(actor::TaskId& task_id,
                       SessionID window_id,
                       bool foreground,
                       ExpectedErrorResult expected_result = {}) {
    // Window_id is passed by value since tests currently only use one window so
    // this allows using browser()->session_id(). Once tests are exercising
    // window creation though this will likely need to become a test-step
    // provided ref.
    auto create_tab_provider =
        base::BindLambdaForTesting([&task_id, window_id, foreground]() {
          Actions create_tab = actor::MakeCreateTab(window_id, foreground);
          create_tab.set_task_id(task_id.value());
          return EncodeActionProto(create_tab);
        });
    return ExecuteAction(std::move(create_tab_provider),
                         std::move(expected_result));
  }

  auto CreateWindowAction(actor::TaskId& task_id,
                          ExpectedErrorResult expected_result = {}) {
    auto create_window_provider = base::BindLambdaForTesting([&task_id]() {
      Actions create_window = actor::MakeCreateWindow();
      create_window.set_task_id(task_id.value());
      return EncodeActionProto(create_window);
    });
    return ExecuteAction(std::move(create_window_provider),
                         std::move(expected_result));
  }

  auto ActivateWindowAction(actor::TaskId& task_id,
                            SessionID& window_id,
                            ExpectedErrorResult expected_result = {}) {
    auto activate_window_provider =
        base::BindLambdaForTesting([&task_id, &window_id]() {
          Actions activate_window = actor::MakeActivateWindow(window_id);
          activate_window.set_task_id(task_id.value());
          return EncodeActionProto(activate_window);
        });
    return ExecuteAction(std::move(activate_window_provider),
                         std::move(expected_result));
  }

  auto CloseWindowAction(actor::TaskId& task_id,
                         SessionID& window_id,
                         ExpectedErrorResult expected_result = {}) {
    auto close_window_provider =
        base::BindLambdaForTesting([&task_id, &window_id]() {
          Actions close_window = actor::MakeCloseWindow(window_id);
          close_window.set_task_id(task_id.value());
          return EncodeActionProto(close_window);
        });
    return ExecuteAction(std::move(close_window_provider),
                         std::move(expected_result));
  }

  auto GetClientRect(ui::ElementIdentifier tab_id,
                     std::string_view element_id,
                     gfx::Rect& out_rect) {
    return Steps(InAnyContext(WithElement(tab_id, [element_id, &out_rect](
                                                      ui::TrackedElement* el) {
      const base::Value result =
          AsInstrumentedWebContents(el)->Evaluate(content::JsReplace(
              "() => document.getElementById($1).getBoundingClientRect().toJSON()",
              element_id));
      out_rect.SetRect(base::ClampRound(*result.GetDict().FindDouble("x")),
                       base::ClampRound(*result.GetDict().FindDouble("y")),
                       base::ClampRound(*result.GetDict().FindDouble("width")),
                       base::ClampRound(*result.GetDict().FindDouble("height")));
    })));
  }

  auto ClickAction(std::string_view label,
                   ClickType click_type,
                   ClickCount click_count,
                   actor::TaskId& task_id,
                   TabHandle& tab_handle,
                   ExpectedErrorResult expected_result = {}) {
    auto click_provider = base::BindLambdaForTesting(
        [this, &task_id, &tab_handle, label, click_type, click_count]() {
          int32_t node_id = SearchAnnotatedPageContent(label);
          RenderFrameHost* frame =
              tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
          Actions action =
              actor::MakeClick(*frame, node_id, click_type, click_count);
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(click_provider), std::move(expected_result));
  }

  auto ClickAction(std::string_view label,
                   ClickType click_type,
                   ClickCount click_count,
                   ExpectedErrorResult expected_result = {}) {
    return ClickAction(label, click_type, click_count, task_id_, tab_handle_,
                       std::move(expected_result));
  }

  auto ClickAction(const gfx::Point& coordinate,
                   ClickType click_type,
                   ClickCount click_count,
                   actor::TaskId& task_id,
                   TabHandle& tab_handle,
                   ExpectedErrorResult expected_result = {}) {
    auto click_provider = base::BindLambdaForTesting(
        [&task_id, &tab_handle, coordinate, click_type, click_count]() {
          Actions action =
              actor::MakeClick(tab_handle, coordinate, click_type, click_count);
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(click_provider), std::move(expected_result));
  }

  auto ClickAction(const gfx::Point& coordinate,
                   ClickType click_type,
                   ClickCount click_count,
                   ExpectedErrorResult expected_result = {}) {
    return ClickAction(coordinate, click_type, click_count, task_id_,
                       tab_handle_, std::move(expected_result));
  }

  auto MouseMoveAction(std::string_view label,
                       actor::TaskId& task_id,
                       TabHandle& tab_handle,
                       ExpectedErrorResult expected_result = {}) {
    auto move_provider =
        base::BindLambdaForTesting([this, &task_id, &tab_handle, label]() {
          int32_t node_id = SearchAnnotatedPageContent(label);
          RenderFrameHost* frame =
              tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
          Actions action = actor::MakeMouseMove(*frame, node_id);
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(move_provider), std::move(expected_result));
  }

  auto MouseMoveAction(std::string_view label,
                       ExpectedErrorResult expected_result = {}) {
    return MouseMoveAction(label, task_id_, tab_handle_,
                           std::move(expected_result));
  }

  auto NavigateAction(GURL url,
                      actor::TaskId& task_id,
                      TabHandle& tab_handle,
                      ExpectedErrorResult expected_result = {}) {
    auto navigate_provider =
        base::BindLambdaForTesting([&task_id, &tab_handle, url]() {
          Actions action = actor::MakeNavigate(tab_handle, url.spec());
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(navigate_provider),
                         std::move(expected_result));
  }

  auto NavigateAction(GURL url, ExpectedErrorResult expected_result = {}) {
    return NavigateAction(url, task_id_, tab_handle_,
                          std::move(expected_result));
  }

  auto HistoryAction(HistoryDirection direction,
                     actor::TaskId& task_id,
                     TabHandle& tab_handle,
                     ExpectedErrorResult expected_result = {}) {
    auto navigate_provider =
        base::BindLambdaForTesting([&task_id, &tab_handle, direction]() {
          Actions action = direction == HistoryDirection::kBack
                               ? actor::MakeHistoryBack(tab_handle)
                               : actor::MakeHistoryForward(tab_handle);
          action.set_task_id(task_id.value());
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(navigate_provider),
                         std::move(expected_result));
  }

  auto HistoryAction(HistoryDirection direction,
                     ExpectedErrorResult expected_result = {}) {
    return HistoryAction(direction, task_id_, tab_handle_,
                         std::move(expected_result));
  }

  auto WaitAction(actor::TaskId& task_id,
                  ExpectedErrorResult expected_result = {}) {
    auto wait_provider = base::BindLambdaForTesting([&task_id]() {
      Actions action = actor::MakeWait();
      action.set_task_id(task_id.value());
      return EncodeActionProto(action);
    });
    return ExecuteAction(std::move(wait_provider), std::move(expected_result));
  }

  auto WaitAction(ExpectedErrorResult expected_result = {}) {
    return WaitAction(task_id_, std::move(expected_result));
  }

  auto ScriptAction(const std::string& name,
                    const std::string& input_arguments) {
    auto script_provider = base::BindLambdaForTesting([&]() {
      Actions action = actor::MakeScriptTool(
          *tab_handle_.Get()->GetContents()->GetPrimaryMainFrame(), name,
          input_arguments);
      action.set_task_id(task_id_.value());
      return EncodeActionProto(action);
    });
    return ExecuteAction(std::move(script_provider), {});
  }

  // Starts a new task by executing an initial navigate action to `task_url` to
  // create a new tab. The new tab can then be referenced by the identifier
  // passed in `new_tab_id`. Stores the created task's id in `task_id_` and the
  // new tab's handle in `tab_handle_`.
  auto StartActorTaskInNewTab(const GURL& task_url,
                              ui::ElementIdentifier new_tab_id) {
    return Steps(
        // clang-format off
      InstrumentNextTab(new_tab_id),
      CreateTask(task_id_),
      CreateTabAction(task_id_,
                      browser()->session_id(),
                      /*foreground=*/true),
      WaitForWebContentsReady(new_tab_id),
      InAnyContext(WithElement(new_tab_id, [this](ui::TrackedElement* el) {
        content::WebContents* new_tab_contents =
            AsInstrumentedWebContents(el)->web_contents();
        TabInterface* tab = TabInterface::GetFromContents(new_tab_contents);
        CHECK(tab);
        tab_handle_ = tab->GetHandle();
      })),
      NavigateAction(task_url,
                     task_id_,
                     tab_handle_),
      WaitForWebContentsReady(new_tab_id, task_url)
        // clang-format on
    );
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
  // TODO(crbug.com/431760051): This needs to use the correct task_id but the
  // implementation of stopActorTask currently ignores the argument.
  auto StopActorTask() {
    return Steps(InAnyContext(WithElement(
                     kGlicContentsElementId,
                     [&task_id = task_id_](ui::TrackedElement* el) {
                       content::WebContents* glic_contents =
                           AsInstrumentedWebContents(el)->web_contents();
                       std::string script = content::JsReplace(
                           "client.browser.stopActorTask($1);",
                           task_id.value());
                       ASSERT_TRUE(content::ExecJs(glic_contents, script));
                     })),
                 RoundTrip());
  }

  // Pauses a running task by calling the glic PauseActorTask API.
  auto PauseActorTask() {
    return Steps(InAnyContext(WithElement(
                     kGlicContentsElementId,
                     [&task_id = task_id_](ui::TrackedElement* el) {
                       content::WebContents* glic_contents =
                           AsInstrumentedWebContents(el)->web_contents();
                       std::string script = content::JsReplace(
                           "client.browser.pauseActorTask($1);",
                           task_id.value());
                       ASSERT_TRUE(content::ExecJs(glic_contents, script));
                     })),
                 RoundTrip());
  }

  // Resumes a paused task by calling the glic ResumeActorTask API.
  auto ResumeActorTask(base::Value::Dict context_options, bool expected) {
    return InAnyContext(CheckElement(
        kGlicContentsElementId,
        [&task_id = task_id_, context_options = std::move(context_options)](
            ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          std::string script = content::JsReplace(
              R"js(
                              (async () => {
                                try {
                                  await client.browser.resumeActorTask($1, $2);
                                  return true;
                                } catch (err) {
                                  return false;
                                }
                              })();
                            )js",
              task_id.value(), std::move(context_options));
          return content::EvalJs(glic_contents, script).ExtractBool();
        },
        expected));
  }

  auto WaitForActorTaskState(mojom::ActorTaskState expected_state) {
    // WaitForActorTaskState doesn't reliably check the stopped state, since the
    // observable may have already been deleted.
    // Use PrepareForStopStateChange/WaitForActorTaskStateToStopped instead.
    EXPECT_NE(expected_state, mojom::ActorTaskState::kStopped);

    return InAnyContext(WithElement(
        kGlicContentsElementId,
        [&task_id = task_id_, expected_state](ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          std::string script = content::JsReplace(
              R"js(
              client.browser.getActorTaskState($1).waitUntil((state) => {
                return state == $2;
              });
              )js",
              task_id.value(), base::to_underlying(expected_state));
          ASSERT_TRUE(content::ExecJs(glic_contents, script));
        }));
  }

  // Gets a reference to a state observable for use in
  // WaitForActorTaskStateToStopped.
  auto PrepareForStopStateChange() {
    return InAnyContext(WithElement(
        kGlicContentsElementId, [&task_id = task_id_](ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          std::string script = content::JsReplace(
              "window.taskStateObs = client.browser.getActorTaskState($1);",
              task_id.value());
          ASSERT_TRUE(content::ExecJs(glic_contents, script));
        }));
  }

  // Uses the state observable from PrepareForStopStateChange to await a state
  // change to stopped.
  auto WaitForActorTaskStateChangeToStopped() {
    return InAnyContext(
        WithElement(kGlicContentsElementId, [](ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          std::string script = content::JsReplace(
              "window.taskStateObs.waitUntil((state) => { "
              "  return state == $1; "
              "});",
              base::to_underlying(mojom::ActorTaskState::kStopped));
          ASSERT_TRUE(content::ExecJs(glic_contents, script));
        }));
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
    return Do([&]() {
      GlicKeyedService* glic_service =
          GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
      ASSERT_TRUE(glic_service);

      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

      auto options = mojom::GetTabContextOptions::New();
      options->include_annotated_page_content = true;
      FocusedTabData data = glic_service->sharing_manager().GetFocusedTabData();
      if (data.focus()) {
        FetchPageContext(
            data.focus(), *options,
            base::BindLambdaForTesting(
                [&](base::expected<
                    glic::mojom::GetContextResultPtr,
                    page_content_annotations::FetchPageContextErrorDetails>
                        result) {
                  mojo_base::ProtoWrapper& serialized_apc =
                      *result.value()
                           ->get_tab_context()
                           ->annotated_page_data->annotated_page_content;
                  annotated_page_content_ =
                      std::make_unique<AnnotatedPageContent>(
                          serialized_apc.As<AnnotatedPageContent>().value());
                  run_loop.Quit();
                }));

        run_loop.Run();
      }
    });
  }

  auto CheckIsActingOnTab(ui::ElementIdentifier tab, bool expected) {
    return InAnyContext(CheckElement(
        tab,
        [](ui::TrackedElement* el) {
          content::WebContents* tab_contents =
              AsInstrumentedWebContents(el)->web_contents();
          auto* actor_service =
              actor::ActorKeyedService::Get(tab_contents->GetBrowserContext());
          return actor_service &&
                 actor_service->IsAnyTaskActingOnTab(
                     *tabs::TabInterface::GetFromContents(tab_contents));
        },
        expected));
  }

  auto CheckIsWebContentsCaptured(ui::ElementIdentifier tab, bool expected) {
    return InAnyContext(CheckElement(
        tab,
        [](ui::TrackedElement* el) {
          content::WebContents* tab_contents =
              AsInstrumentedWebContents(el)->web_contents();
          return tab_contents->IsBeingCaptured();
        },
        expected));
  }

  // Check ExecutionEngine caches the last apc observation.
  auto CheckActorTabDataHasAnnotatedPageContentCache() {
    return Do([&]() {
      const AnnotatedPageContent* cached_apc =
          actor::ActorTabData::From(GetActorTask()->GetTabForObservation())
              ->GetLastObservedPageContent();
      EXPECT_TRUE(cached_apc);
      EXPECT_THAT(*annotated_page_content_, EqualsProto(*cached_apc));
    });
  }

  auto OpenDevToolsWindow(ui::ElementIdentifier contents_to_inspect) {
    return InAnyContext(
        WithElement(contents_to_inspect, [](ui::TrackedElement* el) {
          content::WebContents* contents =
              AsInstrumentedWebContents(el)->web_contents();
          DevToolsWindowTesting::OpenDevToolsWindowSync(contents,
                                                        /*is_docked=*/false);
        }));
  }

  auto CloseTab(ui::ElementIdentifier tab) {
    return InAnyContext(
        WithElement(tab, [this](ui::TrackedElement* el) {
          content::WebContents* contents =
              AsInstrumentedWebContents(el)->web_contents();
          content::WebContentsDestroyedWatcher destroyed_watcher(contents);
          chrome::CloseWebContents(browser(), contents, true);
          destroyed_watcher.Wait();
        }).SetMustRemainVisible(false));
  }

  auto NavigateFrame(ui::ElementIdentifier webcontents_id,
                     const std::string_view frame,
                     const GURL& url) {
    return ExecuteJs(webcontents_id,
                     base::StrCat({"()=>{document.getElementById('", frame,
                                   "').src='", url.spec(), "';}"}));
  }

  const std::optional<ActionsResult>& last_execution_result() const {
    return last_execution_result_;
  }

  // The default task_id and tab created by StartActorTaskInNewTab. Most tests
  // will use these to act in the single tab of a task so these are stored for
  // convenience. More complicated tests involving multiple tasks or tabs will
  // have to manage their own handles/ids.
  actor::TaskId task_id_;
  tabs::TabHandle tab_handle_;

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

  std::optional<ActionsResult> last_execution_result_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AnnotatedPageContent> annotated_page_content_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, CreateTaskAndNavigate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  WaitForWebContentsReady(kNewActorTabId, task_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       CachesLastObservedPageContentAfterActionFinish) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  CheckActorTabDataHasAnnotatedPageContentCache());
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ToctouCheckFailWhenCrossOriginTargetFrameChange) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/two_iframes.html");
  const GURL cross_origin_iframe_url = embedded_test_server()->GetURL(
      "foo.com", "/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    // Initialize the iframes
    ExecuteJs(kNewActorTabId,
              "()=>{topframeLoaded = false; bottomframeLoaded = false;}"),
    NavigateFrame(kNewActorTabId, "topframe", cross_origin_iframe_url),
    NavigateFrame(kNewActorTabId, "bottomframe", cross_origin_iframe_url),
    WaitForJsResult(kNewActorTabId,
                    "()=>{return topframeLoaded && bottomframeLoaded;}"),

    // Click in the top frame. This will extract page context after the click
    // action.
    GetPageContextFromFocusedTab(),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE),

    // Remove the top frame which puts the bottom frame at its former location.
    // Sending a click to the same location should fail the TOCTOU check since
    // the last page context had the removed frame there.
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('topframe').remove();}"),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kFrameLocationChangedSinceObservation)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ToctouCheckFailWhenSameSiteTargetFrameChange) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/two_iframes.html");
  const GURL samesite_iframe_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    // Initialize the iframes
    ExecuteJs(kNewActorTabId,
              "()=>{topframeLoaded = false; bottomframeLoaded = false;}"),
    NavigateFrame(kNewActorTabId, "topframe", samesite_iframe_url),
    NavigateFrame(kNewActorTabId, "bottomframe", samesite_iframe_url),
    WaitForJsResult(kNewActorTabId,
                    "()=>{return topframeLoaded && bottomframeLoaded;}"),

    // Click in the top frame. This will extract page context after the click
    // action.
    GetPageContextFromFocusedTab(),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE),

    // Remove the top frame which puts the bottom frame at its former location.
    // Sending a click to the same location should fail the TOCTOU check since
    // the last page context had the removed frame there.
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('topframe').remove();}"),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kFrameLocationChangedSinceObservation)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ToctouCheckFailWhenNodeRemoved) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  constexpr std::string_view kClickableButtonLabel = "clickable";

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('clickable').remove();}"),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
                    actor::mojom::ActionResultCode::kElementOffscreen)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ToctouCheckFailForCoordinateTargetWhenNodeMoved) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextFromFocusedTab(),
    ClickAction({15, 15}, ClickAction::LEFT, ClickAction::SINGLE),
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('clickable').style.cssText = "
              "'position: relative; left: 20px;'}"),
    ExecuteJs(kNewActorTabId,
              "()=>{const forcelayout = "
              "document.getElementById('clickable').offsetHeight;}"),
    ClickAction(
        {15, 15}, ClickAction::LEFT, ClickAction::SINGLE,
            actor::mojom::ActionResultCode::kObservedTargetElementChanged)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ToctouCheckFailsWhenNodeInteractionPointObscured) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_obscured_element.html");
  constexpr std::string_view kClickableButtonLabel = "target";

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(
        kClickableButtonLabel,
        ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kTargetNodeInteractionPointObscured),
     InAnyContext(WithElement(kNewActorTabId, [](ui::TrackedElement* el) {
        content::WebContents* web_contents =
            AsInstrumentedWebContents(el)->web_contents();
        EXPECT_EQ(false,
          content::EvalJs(web_contents, "target_button_clicked"));
        EXPECT_EQ(false,
          content::EvalJs(web_contents, "obstruction_button_clicked"));
      }))
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       UsesExistingActorTabOnSubsequentNavigate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const GURL second_navigate_url =
      embedded_test_server()->GetURL("/actor/blank.html?second");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  // Now that the task is started in a new tab, do the
                  // second navigation.
                  NavigateAction(second_navigate_url),
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
                  ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                              ClickAction::SINGLE),
                  WaitForJsResult(kNewActorTabId, "expect_single_left_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DblClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                              ClickAction::DOUBLE),
                  WaitForJsResult(kNewActorTabId, "expect_double_left_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, RightClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::RIGHT,
                              ClickAction::SINGLE),
                  WaitForJsResult(kNewActorTabId, "expect_single_right_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DblRightClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::RIGHT,
                              ClickAction::DOUBLE),
                  WaitForJsResult(kNewActorTabId, "expect_double_right_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionProtoInvalid) {
  std::string encodedProto = base::Base64Encode("invalid serialized bytes");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      ExecuteAction(ArbitraryStringProvider(encodedProto),
                    mojom::PerformActionsErrorReason::kInvalidProto));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionTargetNotFound) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  auto click_provider = base::BindLambdaForTesting([this]() {
    RenderFrameHost* frame =
        tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
    Actions action = actor::MakeClick(*frame, kNonExistentContentNodeId,
                                      ClickAction::LEFT, ClickAction::SINGLE);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      ExecuteAction(std::move(click_provider),
                    actor::mojom::ActionResultCode::kInvalidDomNodeId));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       MouseMoveTool_NonExistentNode) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',') }", ""),
      ExecuteAction(
          base::BindLambdaForTesting([this]() {
            RenderFrameHost* frame =
                tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
            Actions action =
                actor::MakeMouseMove(*frame, kNonExistentContentNodeId);
            action.set_task_id(task_id_.value());
            return EncodeActionProto(action);
          }),
          actor::mojom::ActionResultCode::kInvalidDomNodeId));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, MouseMoveTool_Events) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}", ""),
      MouseMoveAction("first"),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}",
                      "mouseenter[DIV#first],mousemove[DIV#first]"),
      ExecuteJs(kNewActorTabId, "()=>{ event_log = []; }"),
      MouseMoveAction("second"),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}",
                      "mouseleave[DIV#first],mouseenter[DIV#second],mousemove["
                      "DIV#second]"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       MouseMoveTool_TargetOutsideViewport) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}", ""),
      WaitForJsResult(kNewActorTabId, "()=>{ return window.scrollY == 0 }"),
      MouseMoveAction("offscreen"),
      WaitForJsResult(kNewActorTabId, "()=>{ return window.scrollY > 0 }"),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}",
                      "mouseenter[DIV#offscreen],mousemove[DIV#offscreen]"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       MouseMoveTool_MoveToCoordinate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  gfx::Rect first_bounds;
  auto move_provider = base::BindLambdaForTesting([this, &first_bounds]() {
    Actions action =
        actor::MakeMouseMove(tab_handle_, first_bounds.CenterPoint());
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',') }", ""),
      GetClientRect(kNewActorTabId, "first", first_bounds),
      ExecuteAction(std::move(move_provider)),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',') }",
                      "mouseenter[DIV#first],mousemove[DIV#first]"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       MouseMoveTool_MoveToCoordinateOffScreen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  gfx::Rect offscreen_bounds;
  auto move_provider = base::BindLambdaForTesting([this, &offscreen_bounds]() {
    Actions action =
        actor::MakeMouseMove(tab_handle_, offscreen_bounds.CenterPoint());
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      GetClientRect(kNewActorTabId, "offscreen", offscreen_bounds),
      ExecuteAction(std::move(move_provider),
                    // While we would expect kCoordinatesOutOfBounds, we don't
                    // keep observation state for coordinates outside of the
                    // viewport. So we treat it as stale.
                    actor::mojom::ActionResultCode::
                        kFrameLocationChangedSinceObservation));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, HistoryTool) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url_1 = embedded_test_server()->GetURL("/actor/blank.html?1");
  const GURL url_2 = embedded_test_server()->GetURL("/actor/blank.html?2");
  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(url_1, kNewActorTabId),
    NavigateAction(url_2),
    HistoryAction(HistoryDirection::kBack),
    WaitForWebContentsReady(kNewActorTabId, url_1),
    HistoryAction(HistoryDirection::kForward),
    WaitForWebContentsReady(kNewActorTabId, url_2)
      // clang-format on
  );
}

// Ensure that a task can be stopped and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, StopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    CheckIsActingOnTab(kNewActorTabId, true),
    StopActorTask(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kTaskWentAway),
    CheckIsActingOnTab(kNewActorTabId, false));
  // clang-format on
}

// Tests that closing a tab that's being acted on stops the associated task.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, StopActorTaskOnTabClose) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    CheckIsActingOnTab(kNewActorTabId, true),
    PrepareForStopStateChange(),
    CloseTab(kNewActorTabId),
    WaitForActorTaskStateChangeToStopped());
  // clang-format on
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
      // clang-format off
    InitializeWithOpenGlicWindow(),

    StartActorTaskInNewTab(task_url, kFirstTabId),
    StopActorTask(),

    // Start, click, stop.
    StartActorTaskInNewTab(task_url, kSecondTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kSecondTabId, "() => button_clicked"),
    StopActorTask(),

    // Start, click, stop.
    StartActorTaskInNewTab(task_url, kThirdTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kThirdTabId, "() => button_clicked"),
    StopActorTask()
      // clang-format on
  );
}

// Ensure that a task can be paused and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    CheckIsActingOnTab(kNewActorTabId, true),

    PauseActorTask(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
                actor::mojom::ActionResultCode::kTaskPaused),

    // Unlike stopping, pausing keeps the task.
    CheckIsActingOnTab(kNewActorTabId, true)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseThenStopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    WaitForActorTaskState(mojom::ActorTaskState::kIdle),

    PauseActorTask(),
    CheckIsActingOnTab(kNewActorTabId, true),
    WaitForActorTaskState(mojom::ActorTaskState::kPaused),

    StopActorTask(),
    CheckIsActingOnTab(kNewActorTabId, false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseAlreadyPausedActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),

    // Ensure pausing twice in a row is a no-op.
    PauseActorTask(),
    PauseActorTask(),
    CheckIsActingOnTab(kNewActorTabId, true)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, PauseThenResumeActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),

    // Reset the flag
    ExecuteJs(kNewActorTabId, "() => { button_clicked = false; }"),

    PauseActorTask(),
    ResumeActorTask(UpdatedContextOptions(), true),
    CheckIsActingOnTab(kNewActorTabId, true),

    // Ensure actions work acter pause and resume.
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked")
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ResumeActorTaskWithoutATask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    StopActorTask(),
    CheckIsActingOnTab(kNewActorTabId, false),

    // Once a task is stopped, it can't be resumed.
    ResumeActorTask(UpdatedContextOptions(), false)
      // clang-format on
  );
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

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, GetPageContextWithoutFocus) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      // After waiting, this should get the context for `kNewActorTabId`, not
      // the currently focused settings page. The choice of the settings page is
      // to make the action fail if we try to fetch the page context of the
      // wrong tab.
      WaitAction());
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, StartTaskWithDevtoolsOpen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  // Ensure a new tab can be created without crashing when the most recently
  // focused browser window is not a normal tabbed browser (e.g. a DevTools
  // window).
  RunTestSequence(InitializeWithOpenGlicWindow(),
                  OpenDevToolsWindow(kGlicContentsElementId),
                  StartActorTaskInNewTab(task_url, kNewActorTabId));
}

// Test that nothing breaks if the first action isn't tab scoped.
// crbug.com/431239173.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, FirstActionIsntTabScoped) {
  // Wait is an example of an action that isn't tab scoped.
  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    CreateTask(task_id_),
    WaitAction()
      // clang-format on
  );
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

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ActuationSucceedsOnBackgroundTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckIsActingOnTab(kNewActorTabId, true),
      CheckIsActingOnTab(kOtherTabId, false),
      StopActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false));
  // clang-format on
}

// Basic test to check that the ActionsResult proto returned from PerformActions
// is filled in with the window and tab observation fields.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       PerformActionsResultObservations) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  // clang-format off
  RunTestSequence(
      // Add an extra tab to ensure that the window's tab list is filled in
      // correctly.
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),

      GetPageContextFromFocusedTab(),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),

      Do([&]() {
        ASSERT_TRUE(last_execution_result());

        // Check that the window observation is filled in correctly.
        ASSERT_EQ(last_execution_result()->windows().size(), 1);
        apc::WindowObservation window = last_execution_result()->windows().at(0);
        EXPECT_EQ(window.id(), browser()->session_id().id());
        EXPECT_EQ(window.activated_tab_id(), tab_handle_.raw_value());
        EXPECT_TRUE(window.active());
        ASSERT_GE(browser()->tab_strip_model()->count(), 2);
        EXPECT_EQ(window.tab_ids().size(), browser()->tab_strip_model()->count());
        for (tabs::TabInterface* tab : *browser()->tab_strip_model()) {
          EXPECT_THAT(window.tab_ids(),
                      testing::Contains(tab->GetHandle().raw_value()));
        }
        EXPECT_EQ(window.tab_ids().size(), browser()->tab_strip_model()->count());

        // Check that the acting tab has an observation that's filled in correctly.
        ASSERT_EQ(last_execution_result()->tabs().size(), 1);
        apc::TabObservation tab = last_execution_result()->tabs().at(0);
        EXPECT_TRUE(tab.has_id());
        EXPECT_EQ(tab.id(), tab_handle_.raw_value());
        EXPECT_TRUE(tab.has_annotated_page_content());
        EXPECT_TRUE(tab.annotated_page_content().has_main_frame_data());
        EXPECT_TRUE(tab.annotated_page_content().has_root_node());
        EXPECT_TRUE(tab.has_screenshot());
        EXPECT_GT(tab.screenshot().size(), 0u);
        EXPECT_TRUE(tab.has_screenshot_mime_type());
        EXPECT_EQ(tab.screenshot_mime_type(), actor::kMimeTypeJpeg);
      })
  );
  // clang-format on
}

// Ensure the time-of-use check can succeed when clicking on a text node rather
// than an element.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, TimeOfUseCheckOnTextNode) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  gfx::Rect checkbox_label_bounds;
  auto click_provider =
      base::BindLambdaForTesting([this, &checkbox_label_bounds]() {
        Actions action =
            actor::MakeClick(tab_handle_, checkbox_label_bounds.CenterPoint(),
                             ClickAction::LEFT, ClickAction::SINGLE);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });
  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      GetPageContextFromFocusedTab(),
      GetClientRect(kNewActorTabId, "checkbox-label", checkbox_label_bounds),
      ExecuteAction(std::move(click_provider)),

      WaitForJsResult(kNewActorTabId, "() => document.getElementById('checkbox').checked")
  );
  // clang-format on
}

// Ensure the time-of-use check can succeed when a click is dispatched to an
// element within a shadow DOM that overlaps its host.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, TimeOfUseCheckOnShadowDom) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  // Load the new page that contains the element with a shadow DOM.
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_shadow_dom.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      GetPageContextFromFocusedTab(),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked === true")
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       ActuationSucceedsOnBackgroundTabAfterPauseAndResume) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      PauseActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false),
      ResumeActorTask(UpdatedContextOptions(), true),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckIsActingOnTab(kNewActorTabId, true),
      CheckIsActingOnTab(kOtherTabId, false),
      StopActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, WindowManagementTools) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP() << "Programmatic window activation doesn't work on wayland and"
                 << "this test checks window activation.";
  }
#endif

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  size_t initial_window_count = 0;
  Browser* initial_window = browser();
  SessionID initial_window_session_id = SessionID::InvalidValue();

  Browser* created_window = nullptr;
  SessionID created_window_session_id = SessionID::InvalidValue();

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      WaitForWebContentsReady(kNewActorTabId, task_url),
      GetPageContextFromFocusedTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      Do([&]() {
        initial_window = BrowserList::GetInstance()->GetLastActive();
        initial_window_session_id = initial_window->session_id();
        initial_window_count = BrowserList::GetInstance()->size();
      }),

      // Create a new window
      CreateWindowAction(task_id_),
      Check([&]() {
              return BrowserList::GetInstance()->size() ==
                  initial_window_count + 1;
          },
          "New window was created"),
      CheckResult([]() { return BrowserList::GetInstance()->GetLastActive(); },
          testing::Ne(initial_window),
          "Last active window was changed"),

      Do([&]() {
        created_window = BrowserList::GetInstance()->GetLastActive();
        created_window_session_id = created_window->session_id();
      }),
      Check([&]() { return created_window->IsActive(); },
          "New window is active"),
      Check([&]() { return !initial_window->IsActive(); },
          "Initial window is inactive"),

      // Activate the initial window
      ActivateWindowAction(task_id_, initial_window_session_id),
      CheckResult(
          []() { return BrowserList::GetInstance()->GetLastActive(); },
          initial_window,
          "Initial window becomes last actived"),
      Check([&]() { return initial_window->IsActive(); },
          "Initial window is active"),
      Check([&]() { return !created_window->IsActive(); },
          "New window is inactive"),

      // Close the new window
      CloseWindowAction(task_id_, created_window_session_id),
      Check([&]() {
              return BrowserList::GetInstance()->size() == initial_window_count;
          },
          "Created window was closed"),
      CheckResult(
          []() { return BrowserList::GetInstance()->GetLastActive(); },
          initial_window,
          "Initial window remains active")
  );
  // clang-format on
}

class GlicActorControllerWithScriptToolsTest
    : public GlicActorControllerUiTest {
 public:
  GlicActorControllerWithScriptToolsTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kScriptTools);
  }
  ~GlicActorControllerWithScriptToolsTest() override = default;

  auto CheckValidResult(const std::string& expected_result) {
    return Steps(Do([&]() {
      ASSERT_TRUE(last_execution_result());
      ASSERT_EQ(last_execution_result()->script_tool_results().size(), 1);

      const auto& result = last_execution_result()->script_tool_results().at(0);
      EXPECT_EQ(result.index_of_script_tool_action(), 0);
      EXPECT_EQ(result.result(), expected_result);
    }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerWithScriptToolsTest, Basic) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/script_tool.html");

  const std::string expected_result = "This is an example sentence.";
  const std::string input_arguments =
      "{ \"text\": \"This is an example sentence.\" }";
  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  ScriptAction("echo", input_arguments),
                  CheckValidResult(expected_result));
}

}  //  namespace

}  // namespace glic::test
