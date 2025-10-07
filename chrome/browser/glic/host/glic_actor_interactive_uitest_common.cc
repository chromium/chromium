// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/interaction/element_identifier.h"

namespace glic::test {

namespace apc = ::optimization_guide::proto;

using ::actor::TaskId;
using apc::Actions;
using apc::ActionsResult;
using apc::AnnotatedPageContent;
using apc::ClickAction;
using ClickType = apc::ClickAction::ClickType;
using ClickCount = apc::ClickAction::ClickCount;
using apc::ContentNode;
using ::content::RenderFrameHost;
using ::content::WebContents;
using ::tabs::TabHandle;
using ::tabs::TabInterface;
using MultiStep = GlicActorUiTest::MultiStep;

// static
std::string GlicActorUiTest::EncodeActionProto(const Actions& action) {
  return base::Base64Encode(action.SerializeAsString());
}

// static
std::optional<ActionsResult> GlicActorUiTest::DecodeActionsResultProto(
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

GlicActorUiTest::GlicActorUiTest() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {// Increase timeout since tests are timing out with ASAN builds.
       {features::kGlic, {{"glic-max-loading-time-ms", "30000"}}},
       {features::kGlicActor, {}},
       {features::kGlicActorToctouValidation, {}},
       {optimization_guide::features::
            kAnnotatedPageContentWithActionableElements,
        {}}},
      /*disabled_features=*/{});
}
GlicActorUiTest::~GlicActorUiTest() = default;

void GlicActorUiTest::SetUpOnMainThread() {
  // Add rule for resolving cross origin host names.
  InteractiveGlicTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
}

const actor::ActorTask* GlicActorUiTest::GetActorTask() {
  auto* actor_service = actor::ActorKeyedService::Get(browser()->profile());
  return actor_service->GetTask(task_id_);
}

MultiStep GlicActorUiTest::ExecuteAction(ActionProtoProvider proto_provider,
                                         ExpectedErrorResult expected_result) {
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
          [](mojom::PerformActionsErrorReason r) { return base::ToString(r); },
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

MultiStep GlicActorUiTest::CreateTask(actor::TaskId& out_task,
                                      std::string_view title) {
  return InAnyContext(WithElement(
      kGlicContentsElementId,
      [&out_task, title = std::string(title)](ui::TrackedElement* el) mutable {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        const int result =
            content::EvalJs(
                glic_contents,
                content::JsReplace("client.browser.createTask({title: $1})",
                                   title))
                .ExtractInt();
        out_task = actor::TaskId(result);
      }));
}

MultiStep GlicActorUiTest::CreateTabAction(
    actor::TaskId& task_id,
    SessionID window_id,
    bool foreground,
    ExpectedErrorResult expected_result) {
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

MultiStep GlicActorUiTest::GetClientRect(ui::ElementIdentifier tab_id,
                                         std::string_view element_id,
                                         gfx::Rect& out_rect) {
  return Steps(InAnyContext(WithElement(tab_id, [element_id, &out_rect](
                                                    ui::TrackedElement* el) {
    const base::Value result =
        AsInstrumentedWebContents(el)->Evaluate(content::JsReplace(
            "() => "
            "document.getElementById($1).getBoundingClientRect().toJSON()",
            element_id));
    out_rect.SetRect(base::ClampRound(*result.GetDict().FindDouble("x")),
                     base::ClampRound(*result.GetDict().FindDouble("y")),
                     base::ClampRound(*result.GetDict().FindDouble("width")),
                     base::ClampRound(*result.GetDict().FindDouble("height")));
  })));
}

MultiStep GlicActorUiTest::ClickAction(std::string_view label,
                                       ClickType click_type,
                                       ClickCount click_count,
                                       actor::TaskId& task_id,
                                       TabHandle& tab_handle,
                                       ExpectedErrorResult expected_result) {
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

MultiStep GlicActorUiTest::ClickAction(std::string_view label,
                                       ClickType click_type,
                                       ClickCount click_count,
                                       ExpectedErrorResult expected_result) {
  return ClickAction(label, click_type, click_count, task_id_, tab_handle_,
                     std::move(expected_result));
}

MultiStep GlicActorUiTest::ClickAction(const gfx::Point& coordinate,
                                       ClickType click_type,
                                       ClickCount click_count,
                                       actor::TaskId& task_id,
                                       TabHandle& tab_handle,
                                       ExpectedErrorResult expected_result) {
  auto click_provider = base::BindLambdaForTesting(
      [&task_id, &tab_handle, coordinate, click_type, click_count]() {
        Actions action =
            actor::MakeClick(tab_handle, coordinate, click_type, click_count);
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(click_provider), std::move(expected_result));
}

MultiStep GlicActorUiTest::ClickAction(const gfx::Point& coordinate,
                                       ClickType click_type,
                                       ClickCount click_count,
                                       ExpectedErrorResult expected_result) {
  return ClickAction(coordinate, click_type, click_count, task_id_, tab_handle_,
                     std::move(expected_result));
}

MultiStep GlicActorUiTest::NavigateAction(GURL url,
                                          actor::TaskId& task_id,
                                          tabs::TabHandle& tab_handle,
                                          ExpectedErrorResult expected_result) {
  auto navigate_provider =
      base::BindLambdaForTesting([&task_id, &tab_handle, url]() {
        optimization_guide::proto::Actions action =
            actor::MakeNavigate(tab_handle, url.spec());
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(navigate_provider),
                       std::move(expected_result));
}

MultiStep GlicActorUiTest::NavigateAction(GURL url,
                                          ExpectedErrorResult expected_result) {
  return NavigateAction(url, task_id_, tab_handle_, std::move(expected_result));
}

MultiStep GlicActorUiTest::StartActorTaskInNewTab(
    const GURL& task_url,
    ui::ElementIdentifier new_tab_id) {
  return Steps(
      // clang-format off
      InstrumentNextTab(new_tab_id),
      CreateTask(task_id_, ""),
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

MultiStep GlicActorUiTest::RoundTrip() {
  return Steps(
      InAnyContext(
          WithElement(kGlicContentsElementId,
                      [](ui::TrackedElement* el) {
                        content::WebContents* glic_contents =
                            AsInstrumentedWebContents(el)->web_contents();
                        ASSERT_TRUE(content::ExecJs(glic_contents, "true;"));
                      })),
      InAnyContext(WithElement(kGlicHostElementId, [](ui::TrackedElement* el) {
        content::WebContents* webui_contents =
            AsInstrumentedWebContents(el)->web_contents();
        ASSERT_TRUE(content::ExecJs(webui_contents, "true;"));
      })));
}

MultiStep GlicActorUiTest::StopActorTask() {
  return Steps(InAnyContext(WithElement(
                   kGlicContentsElementId,
                   [&task_id = task_id_](ui::TrackedElement* el) {
                     content::WebContents* glic_contents =
                         AsInstrumentedWebContents(el)->web_contents();
                     std::string script = content::JsReplace(
                         "client.browser.stopActorTask($1);", task_id.value());
                     ASSERT_TRUE(content::ExecJs(glic_contents, script));
                   })),
               RoundTrip());
}

MultiStep GlicActorUiTest::PauseActorTask() {
  return Steps(InAnyContext(WithElement(
                   kGlicContentsElementId,
                   [&task_id = task_id_,
                    &tab_handle = tab_handle_](ui::TrackedElement* el) {
                     content::WebContents* glic_contents =
                         AsInstrumentedWebContents(el)->web_contents();
                     std::string script = content::JsReplace(
                         "client.browser.pauseActorTask($1, /* pauseReason= "
                         "*/0, /* tabId= */'$2');",
                         task_id.value(), tab_handle.raw_value());
                     ASSERT_TRUE(content::ExecJs(glic_contents, script));
                   })),
               RoundTrip());
}

MultiStep GlicActorUiTest::ResumeActorTask(base::Value::Dict context_options,
                                           bool expected) {
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

MultiStep GlicActorUiTest::WaitForActorTaskState(
    mojom::ActorTaskState expected_state) {
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

MultiStep GlicActorUiTest::PrepareForStopStateChange() {
  return InAnyContext(WithElement(
      kGlicContentsElementId, [&task_id = task_id_](ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        std::string script = content::JsReplace(
            "window.taskStateObs = "
            "client.browser.getActorTaskState($1);",
            task_id.value());
        ASSERT_TRUE(content::ExecJs(glic_contents, script));
      }));
}

MultiStep GlicActorUiTest::WaitForActorTaskStateChangeToStopped() {
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

GlicActorUiTest::ActionProtoProvider GlicActorUiTest::ArbitraryStringProvider(
    std::string_view str) {
  return base::BindLambdaForTesting([str]() { return std::string(str); });
}

base::Value::Dict GlicActorUiTest::UpdatedContextOptions() {
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

MultiStep GlicActorUiTest::InitializeWithOpenGlicWindow() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCurrentActiveTabId);

  // Navigate to ensure the initial tab has some valid content loaded that the
  // Glic window can observe.
  const GURL start_url =
      embedded_test_server()->GetURL("/actor/blank.html?start");

  return Steps(InstrumentTab(kCurrentActiveTabId),
               NavigateWebContents(kCurrentActiveTabId, start_url),
               OpenGlicWindow(GlicWindowMode::kAttached));
}

MultiStep GlicActorUiTest::GetPageContextFromFocusedTab() {
  return Steps(Do([&]() {
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
  }));
}

MultiStep GlicActorUiTest::CheckIsActingOnTab(ui::ElementIdentifier tab,
                                              bool expected) {
  return InAnyContext(CheckElement(
      tab,
      [](ui::TrackedElement* el) {
        content::WebContents* tab_contents =
            AsInstrumentedWebContents(el)->web_contents();
        auto* actor_service =
            actor::ActorKeyedService::Get(tab_contents->GetBrowserContext());
        return actor_service &&
               actor_service->IsActiveOnTab(
                   *tabs::TabInterface::GetFromContents(tab_contents));
      },
      expected));
}

MultiStep GlicActorUiTest::CheckHasTaskForTab(ui::ElementIdentifier tab,
                                              bool expected) {
  return InAnyContext(CheckElement(
      tab,
      [](ui::TrackedElement* el) {
        content::WebContents* tab_contents =
            AsInstrumentedWebContents(el)->web_contents();
        auto* actor_service =
            actor::ActorKeyedService::Get(tab_contents->GetBrowserContext());
        return actor_service &&
               actor_service->GetTaskFromTab(
                   *tabs::TabInterface::GetFromContents(tab_contents));
      },
      expected));
}

MultiStep GlicActorUiTest::CheckIsWebContentsCaptured(ui::ElementIdentifier tab,
                                                      bool expected) {
  return InAnyContext(CheckElement(
      tab,
      [](ui::TrackedElement* el) {
        content::WebContents* tab_contents =
            AsInstrumentedWebContents(el)->web_contents();
        return tab_contents->IsBeingCaptured();
      },
      expected));
}

const std::optional<ActionsResult>& GlicActorUiTest::last_execution_result()
    const {
  return last_execution_result_;
}
int32_t GlicActorUiTest::SearchAnnotatedPageContent(std::string_view label) {
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

}  // namespace glic::test
