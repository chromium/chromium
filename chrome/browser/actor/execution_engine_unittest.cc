// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/autofill_selection_dialog_event_handler.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/javascript_dialog_interceptor.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/execute_js_tool_request.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/http_response_headers.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace actor {

using ::optimization_guide::proto::Actions;
using testing::_;
using testing::Conditional;
using testing::Eq;
using testing::Field;
using testing::Not;
using testing::Property;
using testing::VariantWith;
using ChangeTaskState = ui::UiEventDispatcher::ChangeTaskState;
using StopTask = ui::UiEventDispatcher::StopTask;
using AddTab = ui::UiEventDispatcher::AddTab;
using enum ActorTask::StoppedReason;

namespace {
constexpr int kFakeContentNodeId = 123;
constexpr char kActionResultHistogram[] =
    "Actor.ExecutionEngine.Action.ResultCode";
constexpr char kActorTaskDurationCompletedHistogram[] =
    "Actor.Task.Duration.Completed";
constexpr char kActorTaskCountCompletedHistogram[] =
    "Actor.Task.Count.Completed";
constexpr char kActorClickToolDurationSuccessHistogram[] =
    "Actor.Tools.ExecutionDuration.Click";
constexpr char kActorFakeToolDurationHistogram[] =
    "Actor.Tools.ExecutionDuration.FakeTool";
constexpr char kActorTaskInterruptionCompletedHistogram[] =
    "Actor.Task.Interruptions.Completed";
constexpr char kActorTaskDurationWallClockCompletedHistogram[] =
    "Actor.Task.Duration.WallClock.Completed";
constexpr char kActorTaskDurationVisibleCompletedHistogram[] =
    "Actor.Task.Duration.Visible.Completed";
constexpr char kActorTaskDurationNotVisibleCompletedHistogram[] =
    "Actor.Task.Duration.NotVisible.Completed";

actor::mojom::ActionResultPtr MakeNotImplementedResult() {
  return MakeResult(::actor::mojom::ActionResultCode::kNotImplemented);
}

class MockAutofillSelectionDialogEventHandler
    : public AutofillSelectionDialogEventHandler {
 public:
  MockAutofillSelectionDialogEventHandler() = default;
  ~MockAutofillSelectionDialogEventHandler() override = default;

  MOCK_METHOD(
      bool,
      OnFormPresented,
      (webui::mojom::AutofillSuggestionDialogOnFormPresentedParamsPtr params),
      (override));
  MOCK_METHOD(
      void,
      OnFormPreviewChanged,
      (webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParamsPtr
           params),
      (override));
  MOCK_METHOD(
      bool,
      OnFormConfirmed,
      (webui::mojom::AutofillSuggestionDialogOnFormConfirmedParamsPtr params),
      (override));

  base::WeakPtr<MockAutofillSelectionDialogEventHandler> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAutofillSelectionDialogEventHandler> weak_factory_{
      this};
};

// Reports the page as stable right away and records each wait.
class FakePageStabilityMonitor
    : public page_content_annotations::mojom::PageStabilityMonitor {
 public:
  void NotifyWhenStable(base::TimeDelta observation_delay,
                        NotifyWhenStableCallback callback) override {
    observation_delays_.push_back(observation_delay);
    if (on_next_wait_held_) {
      held_callback_ = std::move(callback);
      std::move(on_next_wait_held_).Run();
      return;
    }
    std::move(callback).Run();
  }

  // Holds the next wait instead of reporting the page as stable, and runs
  // `on_held` from the task that receives it.
  void HoldNextWait(base::OnceClosure on_held) {
    on_next_wait_held_ = std::move(on_held);
  }

  const std::vector<base::TimeDelta>& observation_delays() const {
    return observation_delays_;
  }

 private:
  base::OnceClosure on_next_wait_held_;
  NotifyWhenStableCallback held_callback_;
  std::vector<base::TimeDelta> observation_delays_;
};

class FakeChromeRenderFrame : public chrome::mojom::ChromeRenderFrame {
 public:
  FakeChromeRenderFrame() = default;
  ~FakeChromeRenderFrame() override = default;

  void OverrideBinder(content::RenderFrameHost* rfh) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        rfh->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        chrome::mojom::ChromeRenderFrame::Name_,
        base::BindRepeating(&FakeChromeRenderFrame::Bind,
                            base::Unretained(this)));
  }

  // chrome::mojom::ChromeRenderFrame:
  void SetWindowFeatures(
      blink::mojom::WindowFeaturesPtr window_features) override {}
  void RequestReloadImageForContextNode() override {}
  void RequestBitmapForContextNode(
      RequestBitmapForContextNodeCallback callback) override {}
  void RequestBitmapForContextNodeWithBoundsHint(
      RequestBitmapForContextNodeWithBoundsHintCallback callback) override {}
  void RequestBoundsHintForAllImages(
      RequestBoundsHintForAllImagesCallback callback) override {}
  void RequestImageForContextNode(
      int32_t image_min_area_pixels,
      const gfx::Size& image_max_size_pixels,
      chrome::mojom::ImageFormat image_format,
      int32_t quality,
      RequestImageForContextNodeCallback callback) override {}
  void ExecuteWebUIJavaScript(const std::u16string& javascript) override {}
  void GetMediaFeedURL(GetMediaFeedURLCallback callback) override {}
  void LoadBlockedPlugins(const std::string& identifier) override {}
  void SetShouldDeferMediaLoad(bool should_defer) override {}

  void InitializeTool(actor::mojom::ToolInvocationPtr request,
                      InitializeToolCallback callback) override {
    std::move(callback).Run(
        mojom::InitializeToolResult::NewSuccessPoint(gfx::Point(100, 100)));
  }
  void ExecuteTool(const actor::TaskId& task_id,
                   ExecuteToolCallback callback) override {
    RunNextToolHook();
    std::move(callback).Run(TakeToolResult());
  }
  void set_on_invoke_tool(
      base::RepeatingCallback<void(actor::mojom::ToolInvocationPtr,
                                   InvokeToolCallback)> callback) {
    on_invoke_tool_ = std::move(callback);
  }

  void InvokeTool(actor::mojom::ToolInvocationPtr request,
                  InvokeToolCallback callback) override {
    if (on_invoke_tool_) {
      on_invoke_tool_.Run(std::move(request), std::move(callback));
      return;
    }
    RunNextToolHook();
    std::move(callback).Run(TakeToolResult());
  }
  void StartActorJournal(
      mojo::PendingAssociatedRemote<actor::mojom::JournalClient> client)
      override {}
  void CreatePageStabilityMonitor(
      mojo::PendingReceiver<
          page_content_annotations::mojom::PageStabilityMonitor> monitor,
      const TaskId& task_id,
      bool supports_paint_stability) override {
    ++page_stability_monitor_count_;
    monitor_paint_stability_flags_.push_back(supports_paint_stability);
    if (bind_page_stability_monitors_) {
      page_stability_monitor_receivers_.Add(&page_stability_monitor_,
                                            std::move(monitor));
    }
  }
  void CancelTool(const TaskId& task_id) override {}
  void GetCrossDocumentScriptToolResult(
      const blink::ToolInvocationToken& execution_id,
      GetCrossDocumentScriptToolResultCallback callback) override {
    std::move(callback).Run("");
  }
#if BUILDFLAG(IS_ANDROID)
  void SetCCTClientHeader(const std::string& header) override {}
#endif
#if BUILDFLAG(ENABLE_PDF)
  void PdfPageCaptured(const std::u16string& contents,
                       const std::string& pdf_lang,
                       const GURL& page_url) override {}
#endif

  // Makes the next tool executed in the renderer fail with `code`.
  void FailNextTool(mojom::ActionResultCode code) {
    SetNextToolResult(MakeResult(code));
  }

  void SetNextToolResult(mojom::ActionResultPtr result) {
    next_tool_result_ = std::move(result);
  }

  void SetNextToolHook(base::OnceClosure hook) {
    next_tool_hook_ = std::move(hook);
  }

  // The number of PageStabilityMonitors created, i.e. of observation delayers
  // armed.
  int page_stability_monitor_count() const {
    return page_stability_monitor_count_;
  }

  const std::vector<bool>& monitor_paint_stability_flags() const {
    return monitor_paint_stability_flags_;
  }

  // If set, created PageStabilityMonitors are bound to
  // `page_stability_monitor()`. Otherwise they're dropped, which makes
  // observation delayers skip waiting for page stability.
  void set_bind_page_stability_monitors(bool bind) {
    bind_page_stability_monitors_ = bind;
  }

  FakePageStabilityMonitor& page_stability_monitor() {
    return page_stability_monitor_;
  }
  const FakePageStabilityMonitor& page_stability_monitor() const {
    return page_stability_monitor_;
  }

 private:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>(
                  std::move(handle)));
  }

  void RunNextToolHook() {
    if (next_tool_hook_) {
      std::move(next_tool_hook_).Run();
    }
  }

  mojom::ActionResultPtr TakeToolResult() {
    if (next_tool_result_) {
      return std::move(next_tool_result_);
    }
    return MakeOkResult();
  }

  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;
  base::RepeatingCallback<void(actor::mojom::ToolInvocationPtr,
                               InvokeToolCallback)>
      on_invoke_tool_;
  mojom::ActionResultPtr next_tool_result_;
  base::OnceClosure next_tool_hook_;
  int page_stability_monitor_count_ = 0;
  std::vector<bool> monitor_paint_stability_flags_;
  bool bind_page_stability_monitors_ = false;
  FakePageStabilityMonitor page_stability_monitor_;
  mojo::ReceiverSet<page_content_annotations::mojom::PageStabilityMonitor>
      page_stability_monitor_receivers_;
};

class ExecutionEngineTest : public ChromeRenderViewHostTestHarness {
 public:
  ExecutionEngineTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }
  ~ExecutionEngineTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AssociateTabInterface();

    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher =
        ui::NewMockUiEventDispatcher();
    mock_ui_event_dispatcher_ =
        static_cast<ui::MockUiEventDispatcher*>(ui_event_dispatcher.get());

    ActorTaskOptions options;
    options.delegate = mock_actor_task_delegate_.GetWeakPtr();
    task_ = ActorTask::CreateForTesting(
        *ActorKeyedService::Get(profile()), TaskId(1),
        std::move(ui_event_dispatcher), TestTaskSourceInfo(),
        &no_enterprise_checker_, options);

    ON_CALL(*mock_ui_event_dispatcher_, OnPreTool)
        .WillByDefault(
            UiEventDispatcherCallback<ToolRequest>(base::BindRepeating(
                MakeOkResult, /*requires_page_stabilization=*/true)));
    ON_CALL(*mock_ui_event_dispatcher_, OnPostTool)
        .WillByDefault(
            UiEventDispatcherCallback<ToolRequest>(base::BindRepeating(
                MakeOkResult, /*requires_page_stabilization=*/true)));
    ON_CALL(*mock_ui_event_dispatcher_, OnActorTaskAsyncChange)
        .WillByDefault(UiEventDispatcherCallback<
                       ui::UiEventDispatcher::ActorTaskAsyncChange>(
            base::BindRepeating(MakeOkResult,
                                /*requires_page_stabilization=*/true)));

    ON_CALL(mock_actor_task_delegate_, RequestToShowUserConfirmationDialog)
        .WillByDefault([](TaskId, const url::Origin&, bool,
                          ActorTaskDelegate::UserConfirmationDialogCallback
                              callback) {
          std::move(callback).Run(
              webui::mojom::UserConfirmationDialogResponse::New(
                  webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                      true)));
        });
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(mock_ui_event_dispatcher_);
    mock_ui_event_dispatcher_ = nullptr;
    if (!task_->IsCompleted()) {
      task_->Stop(kTabDetached);
    }
    task_.reset();
    ClearTabInterface();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::OnceCallback<std::unique_ptr<ToolRequest>()> MakeClickCallback(
      int content_node_id) {
    return base::BindLambdaForTesting([this, content_node_id]() {
      std::string document_identifier =
          *optimization_guide::DocumentIdentifierUserData::
              GetDocumentIdentifier(main_rfh()->GetGlobalFrameToken());
      actor::PageTarget target(
          actor::DomNode{.node_id = content_node_id,
                         .document_identifier = document_identifier});
      std::unique_ptr<ToolRequest> request =
          std::make_unique<actor::ClickToolRequest>(
              ActorSurfaceHandle::From(GetTab()->GetHandle()), target,
              mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
      return request;
    });
  }

 protected:
  // Note: action must be generated from a callback because this method
  // navigates the render frame and the generated action must include a
  // document identifier token which is only available after the navigation.
  bool Act(const GURL& url,
           base::OnceCallback<std::unique_ptr<ToolRequest>()> make_action) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
    fake_chrome_render_frame_.OverrideBinder(main_rfh());

    ActResultFuture success;
    std::unique_ptr<ToolRequest> action = std::move(make_action).Run();
    task_->Act(ToRequestList(std::move(action)), success.GetCallback());
    const auto& action_results = success.Get();
    for (const auto& action_result : action_results) {
      if (!IsOk(*action_result.tool_result.deprecated_action_result())) {
        return false;
      }
    }
    return true;
  }

  tabs::MockTabInterface* GetTab() {
    return tab_state_ ? &tab_state_->tab : nullptr;
  }

  void AssociateTabInterface() { tab_state_.emplace(web_contents()); }
  void ClearTabInterface() { tab_state_.reset(); }

  void EmitJavaScriptDialog(std::string_view message) {
    JavaScriptDialogInterceptor::CreateForWebContents(web_contents());
    auto* interceptor =
        JavaScriptDialogInterceptor::FromWebContents(web_contents());
    ASSERT_NE(interceptor, nullptr);
    bool suppressed = true;
    interceptor->RunJavaScriptDialog(
        web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
        base::UTF8ToUTF16(message), u"",
        base::BindLambdaForTesting([](bool, const std::u16string&) {}),
        &suppressed);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;
  FakeChromeRenderFrame fake_chrome_render_frame_;
  std::unique_ptr<ActorTask> task_;
  raw_ptr<ui::MockUiEventDispatcher> mock_ui_event_dispatcher_;
  testing::NiceMock<MockActorTaskDelegate> mock_actor_task_delegate_;

 private:
  std::optional<TestTabState> tab_state_;

  MockPolicyChecker no_enterprise_checker_{
      EnterprisePolicyChecker::UrlBlockReason::kNotBlocked};
};

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActSucceedsOnSupportedUrl DISABLED_ActSucceedsOnSupportedUrl
#else
#define MAYBE_ActSucceedsOnSupportedUrl ActSucceedsOnSupportedUrl
#endif
TEST_F(ExecutionEngineTest, MAYBE_ActSucceedsOnSupportedUrl) {
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnPreTool(Property(&ToolRequest::JournalEvent, Eq("Click")), _))
      .Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnPostTool(Property(&ToolRequest::JournalEvent, Eq("Click")), _))
      .Times(1);
  EXPECT_CALL(
      *mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<ChangeTaskState>(AllOf(
          Field(&ChangeTaskState::old_state, ActorTask::State::kCreated),
          Field(&ChangeTaskState::new_state, ActorTask::State::kActing)))))
      .Times(1);
  EXPECT_CALL(
      *mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<ChangeTaskState>(AllOf(
          Field(&ChangeTaskState::old_state, ActorTask::State::kActing),
          Field(&ChangeTaskState::new_state, ActorTask::State::kReflecting)))));
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskAsyncChange(VariantWith<AddTab>(_), _))
      .Times(1);
  EXPECT_TRUE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kOk, 1);
  histograms_.ExpectTotalCount(kActorClickToolDurationSuccessHistogram, 1);
}

TEST_F(ExecutionEngineTest, ActFailsOnUnsupportedUrl) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool).Times(0);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool).Times(0);
  EXPECT_FALSE(Act(GURL(chrome::kChromeUIVersionURL),
                   MakeClickCallback(kFakeContentNodeId)));
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_UiOnPreToolFails DISABLED_UiOnPreToolFails
#else
#define MAYBE_UiOnPreToolFails UiOnPreToolFails
#endif
TEST_F(ExecutionEngineTest, MAYBE_UiOnPreToolFails) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool)
      .WillOnce(UiEventDispatcherCallback<ToolRequest>(
          base::BindRepeating(MakeNotImplementedResult)));
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool).Times(0);
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kNotImplemented, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_UiOnPostToolFails DISABLED_UiOnPostToolFails
#else
#define MAYBE_UiOnPostToolFails UiOnPostToolFails
#endif
TEST_F(ExecutionEngineTest, MAYBE_UiOnPostToolFails) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool).Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool)
      .WillOnce(UiEventDispatcherCallback<ToolRequest>(
          base::BindRepeating(MakeNotImplementedResult)));
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kNotImplemented, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActFailsWhenAddTabFails DISABLED_ActFailsWhenAddTabFails
#else
#define MAYBE_ActFailsWhenAddTabFails ActFailsWhenAddTabFails
#endif
TEST_F(ExecutionEngineTest, MAYBE_ActFailsWhenAddTabFails) {
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskAsyncChange(VariantWith<AddTab>(_), _))
      .WillOnce(UiEventDispatcherCallback<
                ui::UiEventDispatcher::ActorTaskAsyncChange>(
          base::BindRepeating(MakeNotImplementedResult)));
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));

  // Because AddTab occurs before entering ExecutionEngine, we don't expect a
  // result to be recorded.
  histograms_.ExpectTotalCount(kActionResultHistogram, 0);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActFailsWhenTabDestroyed DISABLED_ActFailsWhenTabDestroyed
#else
#define MAYBE_ActFailsWhenTabDestroyed ActFailsWhenTabDestroyed
#endif
TEST_F(ExecutionEngineTest, MAYBE_ActFailsWhenTabDestroyed) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ActResultFuture result;

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());

  ClearTabInterface();
  DeleteContents();

  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kTabWentAway, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActFailsWithRendererCrashedWhenToolActsOnWebContents \
  DISABLED_ActFailsWithRendererCrashedWhenToolActsOnWebContents
#define MAYBE_ActDoesNotFailWithRendererCrashedWhenToolDoesNotActOnWebContents \
  DISABLED_ActDoesNotFailWithRendererCrashedWhenToolDoesNotActOnWebContents
#else
#define MAYBE_ActFailsWithRendererCrashedWhenToolActsOnWebContents \
  ActFailsWithRendererCrashedWhenToolActsOnWebContents
#define MAYBE_ActDoesNotFailWithRendererCrashedWhenToolDoesNotActOnWebContents \
  ActDoesNotFailWithRendererCrashedWhenToolDoesNotActOnWebContents
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_ActFailsWithRendererCrashedWhenToolActsOnWebContents) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  process()->SimulateCrash();
  ASSERT_TRUE(web_contents()->IsCrashed());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kRendererCrashed);
}

TEST_F(ExecutionEngineTest,
       MAYBE_ActDoesNotFailWithRendererCrashedWhenToolDoesNotActOnWebContents) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  process()->SimulateCrash();
  ASSERT_TRUE(web_contents()->IsCrashed());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action = std::make_unique<ReloadPageToolRequest>(
      ActorSurfaceHandle::From(GetTab()->GetHandle()));
  task_->Act(ToRequestList(action), result.GetCallback());
  const auto& action_results = result.Get();
  ASSERT_FALSE(action_results.empty());
  for (const auto& action_result : action_results) {
    EXPECT_NE(action_result.tool_result.deprecated_action_result()->code,
              mojom::ActionResultCode::kRendererCrashed);
  }
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_CrossOriginNavigationBeforeAction \
  DISABLED_CrossOriginNavigationBeforeAction
#else
#define MAYBE_CrossOriginNavigationBeforeAction \
  CrossOriginNavigationBeforeAction
#endif
TEST_F(ExecutionEngineTest, MAYBE_CrossOriginNavigationBeforeAction) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  ActResultFuture result;
  base::test::TestFuture<void> start_future;
  ExecutionEngineStateWaiter state_waiter(start_future.GetCallback(),
                                          task_->GetExecutionEngine(),
                                          ExecutionEngine::State::kStartAction);
  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(std::move(action)), result.GetCallback());
  ASSERT_TRUE(start_future.Wait());

  // Before the action happens, commit a cross-origin navigation.
  ASSERT_FALSE(result.IsReady());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost:8000/"));

  // TODO(mcnee): We currently just fail, but this should do something more
  // graceful.
  ExpectErrorResult(result, mojom::ActionResultCode::kCrossOriginNavigation);
  histograms_.ExpectUniqueSample(
      kActionResultHistogram, mojom::ActionResultCode::kCrossOriginNavigation,
      1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_CancelOngoingAction DISABLED_CancelOngoingAction
#else
#define MAYBE_CancelOngoingAction CancelOngoingAction
#endif
TEST_F(ExecutionEngineTest, MAYBE_CancelOngoingAction) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  base::test::TestFuture<void> on_destroy_future;
  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      on_invoke_future.GetCallback(), on_destroy_future.GetCallback());

  ActResultFuture result;
  task_->Act(ToRequestList(request), result.GetCallback());

  // Wait for the tool to be invoked, but don't complete it.
  EXPECT_TRUE(on_invoke_future.Wait());

  task_->GetExecutionEngine().CancelOngoingActions(
      mojom::ActionResultCode::kTaskWentAway);

  // The cancellation should destroy the tool.
  EXPECT_TRUE(on_destroy_future.Wait());

  ExpectErrorResult(result, mojom::ActionResultCode::kTaskWentAway);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionPreservesParentMessageWhenEmpty \
  DISABLED_FollowupActionPreservesParentMessageWhenEmpty
#else
#define MAYBE_FollowupActionPreservesParentMessageWhenEmpty \
  FollowupActionPreservesParentMessageWhenEmpty
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_FollowupActionPreservesParentMessageWhenEmpty) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([this](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindOnce([](DeprecatedToolCallback followup_callback) {
                  std::move(followup_callback)
                      .Run(MakeOkResult(/*requires_page_stabilization=*/false));
                }),
                base::DoNothing()));
        std::move(callback).Run(MakeOkResultWithMessage(
            /*requires_page_stabilization=*/false, "Parent message."));
      }),
      base::DoNothing());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_result.deprecated_action_result()->message,
            "Parent message.");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionSetsMessageWhenParentEmpty \
  DISABLED_FollowupActionSetsMessageWhenParentEmpty
#else
#define MAYBE_FollowupActionSetsMessageWhenParentEmpty \
  FollowupActionSetsMessageWhenParentEmpty
#endif
TEST_F(ExecutionEngineTest, MAYBE_FollowupActionSetsMessageWhenParentEmpty) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([this](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindOnce([](DeprecatedToolCallback followup_callback) {
                  std::move(followup_callback)
                      .Run(MakeOkResultWithMessage(
                          /*requires_page_stabilization=*/false,
                          "Followup message."));
                }),
                base::DoNothing()));
        std::move(callback).Run(
            MakeOkResult(/*requires_page_stabilization=*/false));
      }),
      base::DoNothing());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_result.deprecated_action_result()->message,
            "Followup message.");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionOverwritesParentMessageWhenNonEmpty \
  DISABLED_FollowupActionOverwritesParentMessageWhenNonEmpty
#else
#define MAYBE_FollowupActionOverwritesParentMessageWhenNonEmpty \
  FollowupActionOverwritesParentMessageWhenNonEmpty
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_FollowupActionOverwritesParentMessageWhenNonEmpty) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([this](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindOnce([](DeprecatedToolCallback followup_callback) {
                  std::move(followup_callback)
                      .Run(MakeOkResultWithMessage(
                          /*requires_page_stabilization=*/false,
                          "Followup message."));
                }),
                base::DoNothing()));
        std::move(callback).Run(MakeOkResultWithMessage(
            /*requires_page_stabilization=*/false, "Parent message."));
      }),
      base::DoNothing());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_result.deprecated_action_result()->message,
            "Followup message.");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionFailureUsesFollowupErrorMessage \
  DISABLED_FollowupActionFailureUsesFollowupErrorMessage
#else
#define MAYBE_FollowupActionFailureUsesFollowupErrorMessage \
  FollowupActionFailureUsesFollowupErrorMessage
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_FollowupActionFailureUsesFollowupErrorMessage) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([this](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindOnce([](DeprecatedToolCallback followup_callback) {
                  std::move(followup_callback)
                      .Run(MakeResult(mojom::ActionResultCode::kElementDisabled,
                                      /*requires_page_stabilization=*/false,
                                      "Followup failed."));
                }),
                base::DoNothing()));
        std::move(callback).Run(MakeOkResultWithMessage(
            /*requires_page_stabilization=*/false, "Parent message."));
      }),
      base::DoNothing());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_result.deprecated_action_result()->message,
            "Followup failed.");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionFailureClearsParentMessageWhenErrorMessageEmpty \
  DISABLED_FollowupActionFailureClearsParentMessageWhenErrorMessageEmpty
#else
#define MAYBE_FollowupActionFailureClearsParentMessageWhenErrorMessageEmpty \
  FollowupActionFailureClearsParentMessageWhenErrorMessageEmpty
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_FollowupActionFailureClearsParentMessageWhenErrorMessageEmpty) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([this](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindOnce([](DeprecatedToolCallback followup_callback) {
                  std::move(followup_callback)
                      .Run(MakeResult(
                          mojom::ActionResultCode::kElementDisabled));
                }),
                base::DoNothing()));
        std::move(callback).Run(MakeOkResultWithMessage(
            /*requires_page_stabilization=*/false, "Parent message."));
      }),
      base::DoNothing());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_result.deprecated_action_result()->message,
            "");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionPreservesParentMetadata \
  DISABLED_FollowupActionPreservesParentMetadata
#else
#define MAYBE_FollowupActionPreservesParentMetadata \
  FollowupActionPreservesParentMetadata
#endif
TEST_F(ExecutionEngineTest, MAYBE_FollowupActionPreservesParentMetadata) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([this](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindOnce([](DeprecatedToolCallback followup_callback) {
                  std::move(followup_callback)
                      .Run(MakeOkResult(/*requires_page_stabilization=*/false));
                }),
                base::DoNothing()));
        mojom::ActionResultPtr parent_result =
            MakeOkResult(/*requires_page_stabilization=*/true);
        parent_result->attempt_login_status =
            mojom::AttemptLoginStatus::kPasswordManager;
        parent_result->screenshot_policy = mojom::ScreenshotPolicy::kRequired;
        parent_result->page_content_policy =
            mojom::PageContentExtractionPolicy::kRequired;
        std::move(callback).Run(std::move(parent_result));
      }),
      base::DoNothing());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  const mojom::ActionResult& final_result =
      *result.Get()[0].tool_result.deprecated_action_result();
  EXPECT_TRUE(final_result.requires_page_stabilization);
  EXPECT_EQ(final_result.attempt_login_status,
            mojom::AttemptLoginStatus::kPasswordManager);
  EXPECT_EQ(final_result.screenshot_policy, mojom::ScreenshotPolicy::kRequired);
  EXPECT_EQ(final_result.page_content_policy,
            mojom::PageContentExtractionPolicy::kRequired);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FollowupActionMergesAndCapsJavaScriptDialogs \
  DISABLED_FollowupActionMergesAndCapsJavaScriptDialogs
#else
#define MAYBE_FollowupActionMergesAndCapsJavaScriptDialogs \
  FollowupActionMergesAndCapsJavaScriptDialogs
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_FollowupActionMergesAndCapsJavaScriptDialogs) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(GetTab()->GetHandle());

  auto emit_dialogs = [this](std::string_view prefix, size_t count) {
    JavaScriptDialogInterceptor::CreateForWebContents(web_contents());
    auto* interceptor =
        JavaScriptDialogInterceptor::FromWebContents(web_contents());
    ASSERT_NE(interceptor, nullptr);
    for (size_t i = 0; i < count; ++i) {
      bool suppressed = true;
      interceptor->RunJavaScriptDialog(
          web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
          base::UTF8ToUTF16(base::StrCat({prefix, base::NumberToString(i)})),
          u"", base::BindLambdaForTesting([](bool, const std::u16string&) {}),
          &suppressed);
    }
  };

  constexpr size_t kParentDialogs = 35;
  constexpr size_t kFollowupDialogs = 25;
  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindLambdaForTesting(
                    [&](DeprecatedToolCallback followup_callback) {
                      emit_dialogs("followup-", kFollowupDialogs);
                      std::move(followup_callback)
                          .Run(MakeOkResult(
                              /*requires_page_stabilization=*/false));
                    }),
                base::DoNothing(), surface));
        emit_dialogs("parent-", kParentDialogs);
        std::move(callback).Run(
            MakeOkResult(/*requires_page_stabilization=*/false));
      }),
      base::DoNothing(), surface);

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(),
            JavaScriptDialogInterceptor::kMaxInterceptedDialogs);
  for (size_t i = 0; i < kParentDialogs; ++i) {
    EXPECT_EQ(dialogs[i].dialog_message,
              base::StrCat({"parent-", base::NumberToString(i)}));
  }
  const size_t expected_followup =
      JavaScriptDialogInterceptor::kMaxInterceptedDialogs - kParentDialogs;
  for (size_t i = 0; i < expected_followup; ++i) {
    EXPECT_EQ(dialogs[kParentDialogs + i].dialog_message,
              base::StrCat({"followup-", base::NumberToString(i)}));
  }
}

#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_DeferredToolInvokeDrainsDialogsBeforeWaitingOnUser \
  DISABLED_DeferredToolInvokeDrainsDialogsBeforeWaitingOnUser
#else
#define MAYBE_DeferredToolInvokeDrainsDialogsBeforeWaitingOnUser \
  DeferredToolInvokeDrainsDialogsBeforeWaitingOnUser
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_DeferredToolInvokeDrainsDialogsBeforeWaitingOnUser) {
  base::test::ScopedFeatureList features(kGlicDeferActUntilUninterrupted);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(GetTab()->GetHandle());

  auto emit_dialog = [this](std::u16string_view message) {
    JavaScriptDialogInterceptor::CreateForWebContents(web_contents());
    auto* interceptor =
        JavaScriptDialogInterceptor::FromWebContents(web_contents());
    ASSERT_NE(interceptor, nullptr);
    bool suppressed = true;
    interceptor->RunJavaScriptDialog(
        web_contents(), main_rfh(), content::JAVASCRIPT_DIALOG_TYPE_ALERT,
        std::u16string(message), u"",
        base::BindLambdaForTesting([](bool, const std::u16string&) {}),
        &suppressed);
  };

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
        emit_dialog(u"during-tool");
        task_->Interrupt(/*retain_user_control=*/false,
                         ActorTask::InterruptReason::kWaitingUserConfirmation);
        std::move(callback).Run(
            MakeOkResult(/*requires_page_stabilization=*/false));
      }),
      base::DoNothing(), surface);

  base::RunLoop tool_invoke_complete_loop;
  task_->GetExecutionEngine().set_tool_invoke_complete_callback_for_testing(
      tool_invoke_complete_loop.QuitClosure());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  tool_invoke_complete_loop.Run();
  EXPECT_FALSE(result.IsReady());
  EXPECT_EQ(task_->GetState(), ActorTask::State::kWaitingOnUser);

  // Dialogs fired while waiting on the user must not be attributed to the
  // already-finished tool invocation once uninterrupted.
  emit_dialog(u"while-waiting");

  task_->Uninterrupt(ActorTask::State::kActing);
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].dialog_message, "during-tool");

  // Confirm the post-tool dialog remained in the interceptor rather than being
  // silently dropped.
  const auto remaining_dialogs =
      JavaScriptDialogInterceptor::TakeInterceptedDialogs(*web_contents());
  ASSERT_EQ(remaining_dialogs.size(), 1u);
  EXPECT_EQ(remaining_dialogs[0].dialog_message, "while-waiting");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_FailingFollowupActionMergesInterceptedDialogs \
  DISABLED_FailingFollowupActionMergesInterceptedDialogs
#else
#define MAYBE_FailingFollowupActionMergesInterceptedDialogs \
  FailingFollowupActionMergesInterceptedDialogs
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_FailingFollowupActionMergesInterceptedDialogs) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(GetTab()->GetHandle());

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
        task_->GetExecutionEngine().EnqueueFollowupAction(
            std::make_unique<FakeToolRequest>(
                base::BindLambdaForTesting(
                    [&](DeprecatedToolCallback followup_callback) {
                      EmitJavaScriptDialog("followup-failure");
                      std::move(followup_callback)
                          .Run(MakeNotImplementedResult());
                    }),
                base::DoNothing(), surface));
        EmitJavaScriptDialog("parent-ok");
        std::move(callback).Run(
            MakeOkResult(/*requires_page_stabilization=*/false));
      }),
      base::DoNothing(), surface);

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kNotImplemented);
  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 2u);
  EXPECT_EQ(dialogs[0].dialog_message, "parent-ok");
  EXPECT_EQ(dialogs[1].dialog_message, "followup-failure");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ExternalToolFailurePreservesInterceptedDialogs \
  DISABLED_ExternalToolFailurePreservesInterceptedDialogs
#else
#define MAYBE_ExternalToolFailurePreservesInterceptedDialogs \
  ExternalToolFailurePreservesInterceptedDialogs
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_ExternalToolFailurePreservesInterceptedDialogs) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(GetTab()->GetHandle());

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
        EmitJavaScriptDialog("before-external-failure");
        task_->GetExecutionEngine().FailCurrentTool(
            mojom::ActionResultCode::kUrlBlocked);
        std::move(callback).Run(
            MakeOkResult(/*requires_page_stabilization=*/false));
      }),
      base::DoNothing(), surface);

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);
  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].dialog_message, "before-external-failure");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_NullValidationSurfaceFallsBackToLastActuatedSurfaceForDialogs \
  DISABLED_NullValidationSurfaceFallsBackToLastActuatedSurfaceForDialogs
#else
#define MAYBE_NullValidationSurfaceFallsBackToLastActuatedSurfaceForDialogs \
  NullValidationSurfaceFallsBackToLastActuatedSurfaceForDialogs
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_NullValidationSurfaceFallsBackToLastActuatedSurfaceForDialogs) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(GetTab()->GetHandle());

  base::test::TestFuture<mojom::ActionResultPtr> add_result;
  task_->AddActorSurface(surface, /*stop_task_on_detach=*/false,
                         add_result.GetCallback());
  ExpectOkResult(*add_result.Get());
  ASSERT_EQ(task_->GetLastActuatedActorSurface(), surface);

  std::unique_ptr<ToolRequest> request = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
        EmitJavaScriptDialog("fallback-surface");
        std::move(callback).Run(
            MakeOkResult(/*requires_page_stabilization=*/false));
      }),
      base::DoNothing(), ActorSurfaceHandle::Null());
  ASSERT_TRUE(request->GetActorSurfaceForValidation().is_null());

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].dialog_message, "fallback-surface");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ExecuteJsTimeoutPreservesInterceptedDialogs \
  DISABLED_ExecuteJsTimeoutPreservesInterceptedDialogs
#else
#define MAYBE_ExecuteJsTimeoutPreservesInterceptedDialogs \
  ExecuteJsTimeoutPreservesInterceptedDialogs
#endif
TEST_F(ExecutionEngineTest, MAYBE_ExecuteJsTimeoutPreservesInterceptedDialogs) {
  using ProtoDialog =
      optimization_guide::proto::ExecuteJsResult::JavaScriptDialog;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(GetTab()->GetHandle());

  // Declare `held_invoke_callback` before `fake_chrome_render_frame` so the
  // mojo receiver set closes before the un-invoked callback is destroyed.
  FakeChromeRenderFrame::InvokeToolCallback held_invoke_callback;
  FakeChromeRenderFrame fake_chrome_render_frame;
  base::RunLoop invoke_loop;
  fake_chrome_render_frame.set_on_invoke_tool(base::BindLambdaForTesting(
      [&](actor::mojom::ToolInvocationPtr,
          FakeChromeRenderFrame::InvokeToolCallback callback) {
        EmitJavaScriptDialog("before timeout");
        held_invoke_callback = std::move(callback);
        invoke_loop.Quit();
      }));
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  const base::UnguessableToken document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(main_rfh())
              ->token();
  std::unique_ptr<ToolRequest> request = std::make_unique<ExecuteJsToolRequest>(
      surface, document_id,
      "window.alert('before timeout'); new Promise(() => {});",
      /*timeout_ms=*/1000);

  ActResultFuture result;
  task_->Act(ToRequestList(std::move(request)), result.GetCallback());
  invoke_loop.Run();
  EXPECT_FALSE(result.IsReady());

  // Advance mock time past the 1000ms script timeout plus renderer slack so
  // ExecuteJsToolHost deterministically triggers kExecuteJsTimeout.
  task_environment()->FastForwardBy(base::Seconds(3));
  ExpectErrorResult(result, mojom::ActionResultCode::kExecuteJsTimeout);

  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, ExecuteJsToolRequest::kName);
  EXPECT_FALSE(result.Get()[0]
                   .tool_result.deprecated_action_result()
                   ->execute_js_response);
  ASSERT_EQ(result.Get()[0].javascript_dialogs.size(), 1u);
  EXPECT_EQ(result.Get()[0].javascript_dialogs[0].type,
            JavaScriptDialogType::kAlert);
  EXPECT_EQ(result.Get()[0].javascript_dialogs[0].dialog_message,
            "before timeout");

  optimization_guide::proto::ActionsResult actions_result;
  CopyExecuteJsResults(actions_result, result.Get());
  ASSERT_EQ(actions_result.execute_js_results_size(), 1);
  const auto& execute_js_result = actions_result.execute_js_results(0);
  EXPECT_EQ(execute_js_result.index_of_execute_js_action(), 0);
  ASSERT_EQ(execute_js_result.javascript_dialogs_size(), 1);
  EXPECT_EQ(execute_js_result.javascript_dialogs(0).type(),
            ProtoDialog::TYPE_ALERT);
  EXPECT_EQ(execute_js_result.javascript_dialogs(0).dialog_message(),
            "before timeout");
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActorTaskCompletedHistogram DISABLED_ActorTaskCompletedHistogram
#else
#define MAYBE_ActorTaskCompletedHistogram ActorTaskCompletedHistogram
#endif
TEST_F(ExecutionEngineTest, MAYBE_ActorTaskCompletedHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  for (size_t i = 0; i < 2; ++i) {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action =
        MakeClickCallback(kFakeContentNodeId).Run();
    std::unique_ptr<ToolRequest> action2 =
        MakeClickCallback(kFakeContentNodeId).Run();

    base::test::TestFuture<void> future;
    ActorTaskStateWaiter wait_to_act(future.GetCallback(),
                                     *ActorKeyedService::Get(profile()), *task_,
                                     ActorTask::State::kActing);
    task_->Act(ToRequestList(action, action2), result.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Simulate time passing before the task stops
  const base::TimeDelta task_duration = base::Milliseconds(123);
  task_environment()->FastForwardBy(task_duration);

  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskSyncChange(
                  VariantWith<ui::MockUiEventDispatcher::RemoveTab>(_)))
      .Times(testing::AnyNumber());
  EXPECT_CALL(
      *mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<StopTask>(AllOf(
          Field(&StopTask::task_id, task_->id()),
          Field(&StopTask::final_state, ActorTask::State::kFinished),
          Field(&StopTask::title, task_->title()),
          Field(&StopTask::last_acted_on_tab_handle, GetTab()->GetHandle())))))
      .Times(1);

  task_->Stop(kTaskComplete);
  histograms_.ExpectTimeBucketCount(kActorTaskDurationCompletedHistogram,
                                    task_duration, 1);
  histograms_.ExpectBucketCount(kActorTaskCountCompletedHistogram, 4, 1);
  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationWallClockCompletedHistogram, task_duration, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActorTaskCompletedWithPauseHistogram \
  DISABLED_ActorTaskCompletedWithPauseHistogram
#else
#define MAYBE_ActorTaskCompletedWithPauseHistogram \
  ActorTaskCompletedWithPauseHistogram
#endif
TEST_F(ExecutionEngineTest, MAYBE_ActorTaskCompletedWithPauseHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ActResultFuture result;

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());

  // Simulate the first active period
  const base::TimeDelta active_duration1 = base::Milliseconds(100);
  task_environment()->FastForwardBy(active_duration1);

  task_->Pause(/*from_actor=*/true);

  // Time that passes while paused should not be counted.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  task_->Resume();

  // Simulate the second active period
  const base::TimeDelta active_duration2 = base::Milliseconds(50);
  task_environment()->FastForwardBy(active_duration2);

  task_->Stop(kTaskComplete);
  histograms_.ExpectTimeBucketCount(kActorTaskDurationCompletedHistogram,
                                    active_duration1 + active_duration2, 1);
  histograms_.ExpectBucketCount(kActorTaskCountCompletedHistogram, 1, 1);
  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationWallClockCompletedHistogram, base::Milliseconds(650),
      1);
}

class ExecutionEngineStopReasonParamTest
    : public ExecutionEngineTest,
      public testing::WithParamInterface<
          std::tuple<ActorTask::StoppedReason, const char*>> {
 public:
  ExecutionEngineStopReasonParamTest() = default;
};

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_ActorTaskStoppedHistogram DISABLED_ActorTaskStoppedHistogram
#else
#define MAYBE_ActorTaskStoppedHistogram ActorTaskStoppedHistogram
#endif
TEST_P(ExecutionEngineStopReasonParamTest, MAYBE_ActorTaskStoppedHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  for (size_t i = 0; i < 2; ++i) {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action =
        MakeClickCallback(kFakeContentNodeId).Run();

    base::test::TestFuture<void> future;
    ActorTaskStateWaiter wait_to_act(future.GetCallback(),
                                     *ActorKeyedService::Get(profile()), *task_,
                                     ActorTask::State::kActing);
    task_->Act(ToRequestList(action), result.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Simulate time passing before the task is cancelled
  const base::TimeDelta task_duration = base::Milliseconds(456);
  task_environment()->FastForwardBy(task_duration);

  auto [stopped_reason, histogram_suffix] = GetParam();
  task_->Stop(stopped_reason);

  histograms_.ExpectTimeBucketCount(
      base::StrCat({"Actor.Task.Duration.", histogram_suffix}), task_duration,
      1);
  histograms_.ExpectBucketCount(
      base::StrCat({"Actor.Task.Count.", histogram_suffix}), 2, 1);
  histograms_.ExpectTimeBucketCount(
      base::StrCat({"Actor.Task.Duration.WallClock.", histogram_suffix}),
      task_duration, 1);
}

TEST_F(ExecutionEngineTest, ActorTaskCountAndDurationHistograms) {
  // Task in Created state followed by Acting then Reflecting states.
  const base::TimeDelta created_duration = base::Seconds(5);

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action1 =
      MakeClickCallback(kFakeContentNodeId).Run();
  std::unique_ptr<ToolRequest> action2 =
      MakeClickCallback(kFakeContentNodeId).Run();
  std::unique_ptr<ToolRequest> action3 =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_environment()->FastForwardBy(created_duration);

  base::test::TestFuture<void> future;
  ActorTaskStateWaiter wait_to_act(future.GetCallback(),
                                   *ActorKeyedService::Get(profile()), *task_,
                                   ActorTask::State::kActing);
  task_->Act(ToRequestList(action1, action2, action3), result.GetCallback());
  ASSERT_TRUE(future.Wait());

  histograms_.ExpectTimeBucketCount(
      "Actor.Task.StateTransition.Duration.Created", created_duration, 1);
  histograms_.ExpectBucketCount(
      "Actor.Task.StateTransition.ActionCount.Created_Acting", 0, 1);

  // Task in PausedByUser state
  task_->Pause(/*from_actor=*/false);
  histograms_.ExpectBucketCount(
      "Actor.Task.StateTransition.ActionCount.Acting_PausedByUser", 3, 1);

  const base::TimeDelta pause_duration = base::Seconds(7);
  task_environment()->FastForwardBy(pause_duration);

  // Task in Resumed state.
  task_->Resume();
  histograms_.ExpectTimeBucketCount(
      "Actor.Task.StateTransition.Duration.PausedByUser", pause_duration, 1);
  histograms_.ExpectBucketCount(
      "Actor.Task.StateTransition.ActionCount.PausedByUser_Reflecting", 0, 1);

  const base::TimeDelta reflecting_duration = base::Seconds(8);
  task_environment()->FastForwardBy(reflecting_duration);

  // Task in PausedByActor state.
  task_->Pause(/*from_actor=*/true);
  histograms_.ExpectTimeBucketCount(
      "Actor.Task.StateTransition.Duration.Reflecting", reflecting_duration, 1);
  histograms_.ExpectBucketCount(
      "Actor.Task.StateTransition.ActionCount.Reflecting_PausedByActor", 0, 1);

  task_environment()->FastForwardBy(pause_duration);
  // Task in Resumed state.
  task_->Resume();
  histograms_.ExpectTimeBucketCount(
      "Actor.Task.StateTransition.Duration.PausedByActor", pause_duration, 1);
  histograms_.ExpectBucketCount(
      "Actor.Task.StateTransition.ActionCount.PausedByActor_Reflecting", 0, 1);

  // Task in Finished state.
  task_environment()->FastForwardBy(reflecting_duration);
  task_->Stop(kTaskComplete);
  histograms_.ExpectTimeBucketCount(
      "Actor.Task.StateTransition.Duration.Reflecting", reflecting_duration, 2);
  histograms_.ExpectBucketCount(
      "Actor.Task.StateTransition.ActionCount.Reflecting_Finished", 0, 1);
  histograms_.ExpectBucketCount(kActorTaskInterruptionCompletedHistogram, 2, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_LatencyInfoAndActionDurationHistogram \
  DISABLED_LatencyInfoAndActionDurationHistogram
#else
#define MAYBE_LatencyInfoAndActionDurationHistogram \
  LatencyInfoAndActionDurationHistogram
#endif
TEST_F(ExecutionEngineTest, MAYBE_LatencyInfoAndActionDurationHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ActResultFuture result;

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  const base::TimeDelta simulated_duration = base::Milliseconds(150);
  const base::TimeTicks action_start_time = base::TimeTicks::Now();

  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;

  std::unique_ptr<ToolRequest> action = std::make_unique<FakeToolRequest>(
      on_invoke_future.GetCallback(), base::OnceClosure());

  task_->Act(ToRequestList(action), result.GetCallback());

  ASSERT_TRUE(on_invoke_future.Wait());
  ASSERT_FALSE(result.IsReady()) << "Act should not be finished yet.";

  // Fast forward time by the simulated duration before running callback to
  // complete tool invocation.
  task_environment()->FastForwardBy(simulated_duration);
  std::move(on_invoke_future.Take()).Run(MakeOkResult());

  ASSERT_TRUE(result.Wait());
  const auto& action_results = result.Get();
  for (const auto& action_result : action_results) {
    EXPECT_TRUE(IsOk(*action_result.tool_result.deprecated_action_result()));
  }

  const auto& actions_result = action_results;
  EXPECT_EQ(actions_result.size(), 1u);
  EXPECT_EQ(actions_result[0].start_time, action_start_time);
  EXPECT_EQ(actions_result[0].end_time, action_start_time + simulated_duration);

  EXPECT_EQ(actions_result[0].end_time - actions_result[0].start_time,
            simulated_duration);
  histograms_.ExpectTimeBucketCount(kActorFakeToolDurationHistogram,
                                    simulated_duration, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_CompletedWithInterruptHistogram \
  DISABLED_CompletedWithInterruptHistogram
#else
#define MAYBE_CompletedWithInterruptHistogram CompletedWithInterruptHistogram
#endif
TEST_F(ExecutionEngineTest, MAYBE_CompletedWithInterruptHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ActResultFuture result;

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());

  // Simulate the first active period
  const base::TimeDelta active_duration1 = base::Milliseconds(100);
  task_environment()->FastForwardBy(active_duration1);

  task_->Interrupt();

  // Time that passes while paused should not be counted.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  task_->Uninterrupt(ActorTask::State::kReflecting);

  // Simulate the second active period
  const base::TimeDelta active_duration2 = base::Milliseconds(50);
  task_environment()->FastForwardBy(active_duration2);

  task_->Stop(kTaskComplete);
  histograms_.ExpectTimeBucketCount(kActorTaskDurationCompletedHistogram,
                                    active_duration1 + active_duration2, 1);
  histograms_.ExpectBucketCount(kActorTaskCountCompletedHistogram, 1, 1);
  histograms_.ExpectBucketCount(kActorTaskInterruptionCompletedHistogram, 1, 1);
  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationWallClockCompletedHistogram, base::Milliseconds(650),
      1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_VisibleNotVisibleActuationCompletedHistogram \
  DISABLED_VisibleNotVisibleActuationCompletedHistogram
#else
#define MAYBE_VisibleNotVisibleActuationCompletedHistogram \
  VisibleNotVisibleActuationCompletedHistogram
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_VisibleNotVisibleActuationCompletedHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  task_->AddTab(GetTab()->GetHandle(), /*stop_task_on_detach=*/true,
                base::DoNothing());
  web_contents()->WasShown();

  // Simulate visible actuation.
  const base::TimeDelta visible_duration = base::Milliseconds(100);
  task_environment()->FastForwardBy(visible_duration);

  // Deactivate the tab to simulate not-visible actuation.
  web_contents()->WasHidden();

  // Simulate not-visible actuation.
  const base::TimeDelta not_visible_duration = base::Milliseconds(50);
  task_environment()->FastForwardBy(not_visible_duration);

  task_->Stop(kTaskComplete);

  histograms_.ExpectTimeBucketCount(kActorTaskDurationVisibleCompletedHistogram,
                                    visible_duration, 1);

  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationNotVisibleCompletedHistogram, not_visible_duration, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_VisibleNotVisibleActuationStoppedHistogram \
  DISABLED_VisibleNotVisibleActuationStoppedHistogram
#else
#define MAYBE_VisibleNotVisibleActuationStoppedHistogram \
  VisibleNotVisibleActuationStoppedHistogram
#endif
TEST_P(ExecutionEngineStopReasonParamTest,
       MAYBE_VisibleNotVisibleActuationStoppedHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  task_->AddTab(GetTab()->GetHandle(), /*stop_task_on_detach=*/true,
                base::DoNothing());
  web_contents()->WasShown();

  // Simulate visible actuation.
  const base::TimeDelta visible_duration = base::Milliseconds(100);
  task_environment()->FastForwardBy(visible_duration);

  // Deactivate the tab to simulate not-visible actuation.
  web_contents()->WasHidden();

  // Simulate not-visible actuation.
  const base::TimeDelta not_visible_duration = base::Milliseconds(50);
  task_environment()->FastForwardBy(not_visible_duration);

  auto [stopped_reason, histogram_suffix] = GetParam();
  task_->Stop(stopped_reason);

  histograms_.ExpectTimeBucketCount(
      base::StrCat({"Actor.Task.Duration.Visible.", histogram_suffix}),
      visible_duration, 1);
  histograms_.ExpectTimeBucketCount(
      base::StrCat({"Actor.Task.Duration.NotVisible.", histogram_suffix}),
      not_visible_duration, 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_VisibleNotVisibleActuationWithPauseHistogram \
  DISABLED_VisibleNotVisibleActuationWithPauseHistogram
#else
#define MAYBE_VisibleNotVisibleActuationWithPauseHistogram \
  VisibleNotVisibleActuationWithPauseHistogram
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_VisibleNotVisibleActuationWithPauseHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  task_->AddTab(GetTab()->GetHandle(), /*stop_task_on_detach=*/true,
                base::DoNothing());
  web_contents()->WasShown();

  // Simulate visible actuation.
  const base::TimeDelta visible_duration1 = base::Milliseconds(100);
  task_environment()->FastForwardBy(visible_duration1);

  // Pause the task.
  task_->Pause(/*from_actor=*/true);

  // This time should not be counted.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // Resume the task.
  task_->Resume();

  // Simulate more visible actuation.
  const base::TimeDelta visible_duration2 = base::Milliseconds(50);
  task_environment()->FastForwardBy(visible_duration2);

  task_->Stop(kTaskComplete);

  histograms_.ExpectTimeBucketCount(kActorTaskDurationVisibleCompletedHistogram,
                                    visible_duration1 + visible_duration2, 1);
  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationNotVisibleCompletedHistogram, base::Milliseconds(0), 1);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_VisibleNotVisibleActuationWithWaitingHistogram \
  DISABLED_VisibleNotVisibleActuationWithWaitingHistogram
#else
#define MAYBE_VisibleNotVisibleActuationWithWaitingHistogram \
  VisibleNotVisibleActuationWithWaitingHistogram
#endif
TEST_F(ExecutionEngineTest,
       MAYBE_VisibleNotVisibleActuationWithWaitingHistogram) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));
  task_->AddTab(GetTab()->GetHandle(), /*stop_task_on_detach=*/true,
                base::DoNothing());
  web_contents()->WasShown();
  task_->SetState(ActorTask::State::kReflecting);

  // Simulate visible actuation.
  const base::TimeDelta visible_duration1 = base::Milliseconds(100);
  task_environment()->FastForwardBy(visible_duration1);

  // Interrupt the task.
  task_->Interrupt();

  // This time should be counted.
  const base::TimeDelta waiting_duration = base::Milliseconds(500);
  task_environment()->FastForwardBy(waiting_duration);

  // Uninterrupt the task.
  task_->Uninterrupt(ActorTask::State::kReflecting);

  // Simulate more visible actuation.
  const base::TimeDelta visible_duration2 = base::Milliseconds(50);
  task_environment()->FastForwardBy(visible_duration2);

  task_->Stop(kTaskComplete);

  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationVisibleCompletedHistogram,
      visible_duration1 + waiting_duration + visible_duration2, 1);
  histograms_.ExpectTimeBucketCount(
      kActorTaskDurationNotVisibleCompletedHistogram, base::Milliseconds(0), 1);
}

TEST_F(ExecutionEngineTest,
       RequestToShowAutofillSuggestions_DelegatesToActorTaskDelegate) {
  // Get the real ActorKeyedService.
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  // Prepare the data to be sent.
  ExecutionEngine& execution_engine = task_->GetExecutionEngine();
  std::vector<autofill::ActorFormFillingRequest> test_requests;
  test_requests.emplace_back().requested_data =
      autofill::ActorFormFillingRequestedData::kAddress;

  // Hold the forwarded value in `received_requests`.
  std::vector<autofill::ActorFormFillingRequest> received_requests;
  base::WeakPtr<AutofillSelectionDialogEventHandler> received_handler;

  MockAutofillSelectionDialogEventHandler event_handler;

  // Expect the call to be forwarded to the task's ActorTaskDelegate.
  EXPECT_CALL(mock_actor_task_delegate_,
              RequestToShowAutofillSuggestionsDialog(task_->id(), _, _, _))
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&received_requests),
                               testing::SaveArg<2>(&received_handler)));

  // Call the method under test on the ExecutionEngine.
  execution_engine.RequestToShowAutofillSuggestions(
      test_requests, event_handler.GetWeakPtr(), base::DoNothing());

  // The vector of requests broadcast by the service should match what we sent.
  ASSERT_EQ(received_requests.size(), 1u);
  EXPECT_EQ(received_requests[0].requested_data,
            autofill::ActorFormFillingRequestedData::kAddress);
  EXPECT_EQ(received_handler.get(), &event_handler);
}

struct TestToolData : public ToolResultData {
  explicit TestToolData(int value) : value(value) {}
  ~TestToolData() override = default;

  base::DictValue ToDict() const override {
    return base::DictValue().Set("value", value);
  }

  std::unique_ptr<ToolResultData> Clone() const override {
    return std::make_unique<TestToolData>(*this);
  }

  int value = 0;
};

TEST_F(ExecutionEngineTest, ToolResultSuccessWithData) {
  std::unique_ptr<ToolRequest> action =
      std::make_unique<FakeToolRequest>(ToolResult::Success(TestToolData(42)));

  ActResultFuture result;
  task_->Act(ToRequestList(action), result.GetCallback());

  ASSERT_TRUE(result.Wait());
  const auto& action_results = result.Get();
  ASSERT_EQ(action_results.size(), 1u);
  EXPECT_TRUE(action_results[0].tool_result.Ok());
  ASSERT_TRUE(action_results[0].tool_result.deprecated_action_result());
  EXPECT_TRUE(IsOk(*action_results[0].tool_result.deprecated_action_result()));
  ASSERT_NE(action_results[0].tool_result.data(), nullptr);
  const TestToolData* data =
      action_results[0].tool_result.data()->As<TestToolData>();
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->value, 42);
}

TEST_F(ExecutionEngineTest, ToolResultError) {
  std::unique_ptr<ToolRequest> action =
      std::make_unique<FakeToolRequest>(ToolResult::Error(
          ToolError(mojom::ActionResultCode::kTabWentAway, "No matching tab")));

  ActResultFuture result;
  task_->Act(ToRequestList(action), result.GetCallback());

  ASSERT_TRUE(result.Wait());
  const auto& action_results = result.Get();
  ASSERT_EQ(action_results.size(), 1u);
  EXPECT_FALSE(action_results[0].tool_result.Ok());
  ASSERT_TRUE(action_results[0].tool_result.deprecated_action_result());
  EXPECT_EQ(action_results[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(action_results[0].tool_result.deprecated_action_result()->message,
            "No matching tab");
  ASSERT_TRUE(action_results[0].tool_result.error().has_value());
  EXPECT_EQ(action_results[0].tool_result.error()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(action_results[0].tool_result.error()->message, "No matching tab");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExecutionEngineStopReasonParamTest,
    testing::Values(std::make_tuple(kStoppedByUser, "Cancelled"),
                    std::make_tuple(kTaskComplete, "Completed"),
                    std::make_tuple(kModelError, "ModelError"),
                    std::make_tuple(kChromeFailure, "ChromeFailure"),
                    std::make_tuple(kTabDetached, "TabDetached"),
                    std::make_tuple(kShutdown, "Shutdown"),
                    std::make_tuple(kUserStartedNewChat, "NewChat"),
                    std::make_tuple(kUserLoadedPreviousChat, "PreviousChat")));

class ToolControllerDeferredObservationTest : public ExecutionEngineTest {
 public:
  ToolControllerDeferredObservationTest() {
    // This suite drives ToolController directly without going through
    // ExecutionEngine::Act(), so disable the subframe-gating dry run that
    // queries ExecutionEngine::GetInProgressAction().
    subframe_gating_feature_list_.InitAndDisableFeature(
        kGlicActorSubframeGatingDryRun);
  }

  void SetUp() override {
    ExecutionEngineTest::SetUp();
    // Browser-initiated navigations preconnect a socket when preloading is
    // enabled. This suite waits long enough for that connect to run, which
    // crashes on Android where the in-process network service runs on the UI
    // thread.
    prefetch::SetPreloadPagesState(profile()->GetPrefs(),
                                   prefetch::PreloadPagesState::kNoPreloading);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("http://localhost/"));
    fake_chrome_render_frame_.OverrideBinder(main_rfh());
    fake_chrome_render_frame_.set_bind_page_stability_monitors(true);
    tool_controller_ =
        std::make_unique<ToolController>(*task_, task_->GetExecutionEngine());
  }

  void TearDown() override {
    tool_controller_.reset();
    ExecutionEngineTest::TearDown();
  }

 protected:
  std::unique_ptr<ClickToolRequest> MakeClickRequest(
      base::TimeDelta start_delay) {
    std::string doc_id =
        *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
            main_rfh()->GetGlobalFrameToken());
    return std::make_unique<ClickToolRequest>(
        ActorSurfaceHandle::From(GetTab()->GetHandle()),
        PageTarget(DomNode{.node_id = kFakeContentNodeId,
                           .document_identifier = doc_id}),
        mojom::ClickType::kLeft, mojom::ClickCount::kSingle,
        /*requires_opening_web_contents=*/false,
        ObservationDelayController::PageStabilityConfig{.start_delay =
                                                            start_delay});
  }

  mojom::ActionResultPtr CreateAndInvoke(
      const ToolRequest& request,
      std::optional<ObservationDelayController::PageStabilityConfig>
          deferred_observation_config = std::nullopt) {
    base::test::TestFuture<mojom::ActionResultPtr> created;
    tool_controller_->CreateToolAndValidate(request, created.GetCallback());
    if (mojom::ActionResultPtr res = created.Take(); !IsOk(*res)) {
      return res;
    }
    base::test::TestFuture<ToolResult> invoked;
    tool_controller_->Invoke(invoked.GetCallback(),
                             std::move(deferred_observation_config));
    return std::move(invoked.Take().deprecated_action_result());
  }

  const std::vector<base::TimeDelta>& observation_delays() const {
    return fake_chrome_render_frame_.page_stability_monitor()
        .observation_delays();
  }

  base::test::ScopedFeatureList subframe_gating_feature_list_;
  std::unique_ptr<ToolController> tool_controller_;
};

TEST_F(ToolControllerDeferredObservationTest, NextToolReusesDeferredDelayer) {
  auto click1 = MakeClickRequest(base::Seconds(1));
  auto click2 = MakeClickRequest(base::Seconds(2));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click1, click1->GetObservationPageStabilityConfig())));
  EXPECT_TRUE(tool_controller_->HasPendingObservation());
  EXPECT_THAT(observation_delays(), testing::IsEmpty());

  EXPECT_FALSE(tool_controller_->ShouldSettlePendingObservationBefore(*click2));
  ASSERT_TRUE(IsOk(*CreateAndInvoke(*click2)));
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 1);
  EXPECT_THAT(observation_delays(), testing::ElementsAre(base::Seconds(2)));
}

TEST_F(ToolControllerDeferredObservationTest, SettlePendingObservation) {
  auto click1 = MakeClickRequest(base::Seconds(1));
  auto click2 = MakeClickRequest(base::Seconds(2));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click1, click1->GetObservationPageStabilityConfig())));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click2, click2->GetObservationPageStabilityConfig())));
  ASSERT_TRUE(tool_controller_->HasPendingObservation());

  base::test::TestFuture<void> settled;
  tool_controller_->SettlePendingObservation(settled.GetCallback());
  ASSERT_TRUE(settled.Wait());
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 1);
  EXPECT_THAT(observation_delays(), testing::ElementsAre(base::Seconds(2)));
}

TEST_F(ToolControllerDeferredObservationTest, CancelDropsPendingObservation) {
  auto click = MakeClickRequest(base::Seconds(1));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click, click->GetObservationPageStabilityConfig())));
  tool_controller_->Cancel();
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  task_environment()->RunUntilIdle();
  EXPECT_THAT(observation_delays(), testing::IsEmpty());

  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click, click->GetObservationPageStabilityConfig())));
  bool settled = false;
  tool_controller_->SettlePendingObservation(
      base::BindLambdaForTesting([&]() { settled = true; }));
  tool_controller_->Cancel();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(settled);
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
}

TEST_F(ToolControllerDeferredObservationTest, ShouldSettleBefore) {
  auto click = MakeClickRequest(base::Seconds(1));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click, click->GetObservationPageStabilityConfig())));
  EXPECT_FALSE(tool_controller_->ShouldSettlePendingObservationBefore(
      *MakeClickRequest(base::Seconds(1))));
  FakeToolRequest non_page_request(
      base::DoNothing(), base::DoNothing(),
      ActorSurfaceHandle::From(GetTab()->GetHandle()));
  EXPECT_TRUE(
      tool_controller_->ShouldSettlePendingObservationBefore(non_page_request));

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://localhost/#foo"), main_rfh())
      ->CommitSameDocument();
  EXPECT_TRUE(tool_controller_->ShouldSettlePendingObservationBefore(
      *MakeClickRequest(base::Seconds(1))));
}

TEST_F(ToolControllerDeferredObservationTest, FailedToolHandling) {
  auto click1 = MakeClickRequest(base::Seconds(1));
  fake_chrome_render_frame_.FailNextTool(
      mojom::ActionResultCode::kNotImplemented);
  EXPECT_EQ(
      CreateAndInvoke(*click1, click1->GetObservationPageStabilityConfig())
          ->code,
      mojom::ActionResultCode::kNotImplemented);
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  EXPECT_THAT(observation_delays(), testing::IsEmpty());

  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click1, click1->GetObservationPageStabilityConfig())));
  fake_chrome_render_frame_.FailNextTool(
      mojom::ActionResultCode::kNotImplemented);
  EXPECT_EQ(CreateAndInvoke(*MakeClickRequest(base::Seconds(2)))->code,
            mojom::ActionResultCode::kNotImplemented);
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  EXPECT_THAT(observation_delays(), testing::ElementsAre(base::Seconds(2)));
}

TEST_F(ToolControllerDeferredObservationTest,
       CreateToolFailurePreservesPendingObservation) {
  auto click1 = MakeClickRequest(base::Seconds(1));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click1, click1->GetObservationPageStabilityConfig())));
  ASSERT_TRUE(tool_controller_->HasPendingObservation());

  // A request targeting a closed surface fails synchronously in CreateTool()
  // without consuming the deferred observation.
  ActorSurfaceHandle gone_surface;
  {
    std::unique_ptr<content::WebContents> closed_contents =
        CreateTestWebContents();
    TestTabState closed_tab(closed_contents.get());
    gone_surface = ActorSurfaceHandle::From(closed_tab.tab.GetHandle());
  }
  ASSERT_FALSE(gone_surface.is_null());
  ASSERT_EQ(gone_surface.Get(), nullptr);

  ClickToolRequest invalid_click(gone_surface, PageTarget(gfx::Point(10, 10)),
                                 mojom::ClickType::kLeft,
                                 mojom::ClickCount::kSingle);
  EXPECT_EQ(CreateAndInvoke(invalid_click)->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_TRUE(tool_controller_->HasPendingObservation());

  base::test::TestFuture<void> settled;
  tool_controller_->SettlePendingObservation(settled.GetCallback());
  ASSERT_TRUE(settled.Wait());
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  EXPECT_THAT(observation_delays(), testing::ElementsAre(base::Seconds(1)));
}

TEST_F(ToolControllerDeferredObservationTest,
       SettlePendingObservationRestartsOnNavigation) {
  auto click = MakeClickRequest(base::Seconds(1));
  ASSERT_TRUE(IsOk(
      *CreateAndInvoke(*click, click->GetObservationPageStabilityConfig())));
  ASSERT_TRUE(tool_controller_->HasPendingObservation());

  // Quit from the task that holds the wait. Waiting for the thread to go idle
  // instead would let mock time fast-forward to the observation timeout.
  base::RunLoop wait_held;
  fake_chrome_render_frame_.page_stability_monitor().HoldNextWait(
      wait_held.QuitClosure());
  base::test::TestFuture<void> settled;
  tool_controller_->SettlePendingObservation(settled.GetCallback());
  wait_held.Run();
  EXPECT_FALSE(settled.IsReady());

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://localhost/next"), main_rfh());
  navigation->ReadyToCommit();
  fake_chrome_render_frame_.OverrideBinder(
      navigation->GetFinalRenderFrameHost());
  navigation->Commit();

  ASSERT_TRUE(settled.Wait());
  EXPECT_FALSE(tool_controller_->HasPendingObservation());
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 2);
  EXPECT_EQ(observation_delays().size(), 2u);
}

class ExecutionEngineStabilizeOncePerBundleTest : public ExecutionEngineTest {
 public:
  ExecutionEngineStabilizeOncePerBundleTest() {
    bundle_feature_list_.InitAndEnableFeature(kActorStabilizeOncePerBundle);
  }

  void SetUp() override {
    ExecutionEngineTest::SetUp();
    // Browser-initiated navigations preconnect a socket when preloading is
    // enabled, which crashes on Android where the in-process network service
    // runs on the UI thread.
    prefetch::SetPreloadPagesState(profile()->GetPrefs(),
                                   prefetch::PreloadPagesState::kNoPreloading);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("http://localhost/"));
    fake_chrome_render_frame_.OverrideBinder(main_rfh());
    fake_chrome_render_frame_.set_bind_page_stability_monitors(true);
  }

 protected:
  PageTarget MainFrameNodeTarget(int node_id = kFakeContentNodeId) {
    std::string doc_id =
        *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
            main_rfh()->GetGlobalFrameToken());
    return PageTarget(
        DomNode{.node_id = node_id, .document_identifier = doc_id});
  }

  std::unique_ptr<ToolRequest> MakeBundleClickRequest(
      base::TimeDelta start_delay,
      bool supports_paint_stability = true,
      mojom::ClickType click_type = mojom::ClickType::kLeft,
      bool requires_opening_web_contents = false) {
    return std::make_unique<ClickToolRequest>(
        ActorSurfaceHandle::From(GetTab()->GetHandle()), MainFrameNodeTarget(),
        click_type, mojom::ClickCount::kSingle, requires_opening_web_contents,
        ObservationDelayController::PageStabilityConfig{
            .supports_paint_stability = supports_paint_stability,
            .start_delay = start_delay});
  }

  std::unique_ptr<ToolRequest> MakeBundleSelectRequest(
      std::string_view value = "opt") {
    return std::make_unique<SelectToolRequest>(
        ActorSurfaceHandle::From(GetTab()->GetHandle()), MainFrameNodeTarget(),
        value);
  }

  const std::vector<base::TimeDelta>& observation_delays() const {
    return fake_chrome_render_frame_.page_stability_monitor()
        .observation_delays();
  }

 private:
  base::test::ScopedFeatureList bundle_feature_list_;
};

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       WaitsOnceAtEndOfPageActionBundle) {
  {
    base::test::ScopedFeatureList disabled;
    disabled.InitAndDisableFeature(kActorStabilizeOncePerBundle);
    ActResultFuture result;
    task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                             MakeBundleClickRequest(base::Seconds(2))),
               result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 2);
    EXPECT_THAT(observation_delays(),
                testing::ElementsAre(base::Seconds(1), base::Seconds(2)));
    histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 2);
  }

  ActResultFuture result;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(base::Seconds(2)),
                           MakeBundleClickRequest(base::Seconds(3))),
             result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 3);
  EXPECT_THAT(fake_chrome_render_frame_.monitor_paint_stability_flags(),
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observation_delays(),
              testing::ElementsAre(base::Seconds(1), base::Seconds(2),
                                   base::Seconds(3)));
  // Click1 and Click2 defer and aren't recorded; Click3 runs the shared wait
  // and is recorded (on top of the 2 samples from the disabled run).
  histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 3);
}

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       PaintStabilityRequiresAllActionsInBundleToSupportIt) {
  ActResultFuture click_then_select;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleSelectRequest()),
             click_then_select.GetCallback());
  ExpectOkResult(click_then_select);

  ActResultFuture select_then_click;
  task_->Act(ToRequestList(MakeBundleSelectRequest(),
                           MakeBundleClickRequest(base::Seconds(2))),
             select_then_click.GetCallback());
  ExpectOkResult(select_then_click);

  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 2);
  EXPECT_THAT(fake_chrome_render_frame_.monitor_paint_stability_flags(),
              testing::ElementsAre(false, false));
  EXPECT_THAT(observation_delays(),
              testing::ElementsAre(base::TimeDelta(), base::Seconds(2)));
}

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       NonPageFollowupAndTabBoundariesSettleBeforeSplittingBundle) {
  std::vector<base::TimeDelta> delays_at_non_page;
  std::unique_ptr<ToolRequest> non_page = std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback cb) {
        delays_at_non_page = observation_delays();
        std::move(cb).Run(MakeOkResult());
      }),
      base::DoNothing(), ActorSurfaceHandle::From(GetTab()->GetHandle()));
  ActResultFuture res1;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(base::Seconds(2)),
                           std::move(non_page),
                           MakeBundleClickRequest(base::Seconds(3)),
                           MakeBundleClickRequest(base::Seconds(4))),
             res1.GetCallback());
  ExpectOkResult(res1);
  EXPECT_THAT(delays_at_non_page, testing::ElementsAre(base::Seconds(2)));
  EXPECT_THAT(observation_delays(),
              testing::ElementsAre(base::Seconds(2), base::Seconds(4)));
  // Click1 and Click3 defer and aren't recorded; Click2 and Click4 run the
  // shared waits and are recorded.
  histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.FakeTool", 1);
  histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 2);

  fake_chrome_render_frame_.SetNextToolHook(base::BindLambdaForTesting([&]() {
    task_->GetExecutionEngine().EnqueueFollowupAction(
        MakeBundleClickRequest(base::Seconds(5)));
  }));
  ActResultFuture res2;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(base::Seconds(2))),
             res2.GetCallback());
  ExpectOkResult(res2);
  EXPECT_THAT(
      observation_delays(),
      testing::ElementsAre(base::Seconds(2), base::Seconds(4), base::Seconds(1),
                           base::Seconds(5), base::Seconds(2)));
  // Click1 defers then settles before the follow-up, so it isn't recorded; the
  // follow-up and Click2 run standalone and are recorded.
  histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 4);

  // Switching tabs mid-sequence settles the first tab's bundle before acting on
  // the second tab.
  std::unique_ptr<content::WebContents> second_contents =
      CreateTestWebContents();
  TestTabState second_tab(second_contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      second_contents.get(), GURL("http://localhost/other"));
  fake_chrome_render_frame_.OverrideBinder(
      second_contents->GetPrimaryMainFrame());
  std::string second_doc_id =
      *optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          second_contents->GetPrimaryMainFrame()->GetGlobalFrameToken());
  std::unique_ptr<ToolRequest> second_tab_click =
      std::make_unique<ClickToolRequest>(
          ActorSurfaceHandle::From(second_tab.tab.GetHandle()),
          PageTarget(DomNode{.node_id = kFakeContentNodeId,
                             .document_identifier = second_doc_id}),
          mojom::ClickType::kLeft, mojom::ClickCount::kSingle,
          /*requires_opening_web_contents=*/false,
          ObservationDelayController::PageStabilityConfig{
              .supports_paint_stability = true,
              .start_delay = base::Seconds(6)});
  ActResultFuture res3;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(base::Seconds(2)),
                           std::move(second_tab_click)),
             res3.GetCallback());
  ExpectOkResult(res3);
  EXPECT_THAT(
      observation_delays(),
      testing::ElementsAre(base::Seconds(2), base::Seconds(4), base::Seconds(1),
                           base::Seconds(5), base::Seconds(2), base::Seconds(2),
                           base::Seconds(6)));
  // In res3, Click1 defers and isn't recorded; Click2 runs the shared wait and
  // second_tab_click runs standalone on the other tab, so both are recorded.
  histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 6);
  task_->Stop(kTaskComplete);
}

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       NavigationMidBundleAndBoundaryParam) {
  fake_chrome_render_frame_.SetNextToolHook(base::BindLambdaForTesting([&]() {
    content::NavigationSimulator::CreateRendererInitiated(
        GURL("http://localhost/#section"), main_rfh())
        ->CommitSameDocument();
  }));
  ActResultFuture res1;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(base::Seconds(2))),
             res1.GetCallback());
  ExpectOkResult(res1);
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 2);
  EXPECT_THAT(observation_delays(),
              testing::ElementsAre(base::Seconds(1), base::Seconds(2)));

  base::test::ScopedFeatureList params;
  params.InitAndEnableFeatureWithParameters(
      kActorStabilizeOncePerBundle,
      {{kActorBundleSameDocumentNavigationIsBoundary.name, "false"}});
  fake_chrome_render_frame_.SetNextToolHook(base::BindLambdaForTesting([&]() {
    content::NavigationSimulator::CreateRendererInitiated(
        GURL("http://localhost/#step2"), main_rfh())
        ->CommitSameDocument();
  }));
  ActResultFuture res2;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(3)),
                           MakeBundleClickRequest(base::Seconds(4))),
             res2.GetCallback());
  ExpectOkResult(res2);
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 3);
  EXPECT_THAT(observation_delays(),
              testing::ElementsAre(base::Seconds(1), base::Seconds(2),
                                   base::Seconds(4)));
}

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       NoOpLastActionStillWaitsForEarlierDeferredActions) {
  fake_chrome_render_frame_.SetNextToolHook(base::BindLambdaForTesting([&]() {
    fake_chrome_render_frame_.SetNextToolResult(
        MakeOkResult(/*requires_page_stabilization=*/false));
  }));
  ActResultFuture result;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(base::Seconds(2))),
             result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 1);
  EXPECT_THAT(observation_delays(), testing::ElementsAre(base::Seconds(2)));
}

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       EarlyTerminationSettlesOrPauseCancelsPendingObservation) {
  // Action 2 fails mid-bundle: action 1's pending wait settles and action 3
  // never runs.
  ActResultFuture toctou_res;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                           MakeBundleClickRequest(
                               base::Seconds(2),
                               /*supports_paint_stability=*/true,
                               mojom::ClickType::kLeftOnOccludedTarget),
                           MakeBundleClickRequest(base::Seconds(3))),
             toctou_res.GetCallback());
  ASSERT_EQ(toctou_res.Get().size(), 2u);
  EXPECT_TRUE(toctou_res.Get()[0].tool_result.Ok());
  EXPECT_FALSE(toctou_res.Get()[1].tool_result.Ok());
  EXPECT_EQ(fake_chrome_render_frame_.page_stability_monitor_count(), 1);
  EXPECT_THAT(observation_delays(), testing::ElementsAre(base::Seconds(1)));

  fake_chrome_render_frame_.SetNextToolHook(base::BindLambdaForTesting([&]() {
    task_->GetExecutionEngine().FailCurrentTool(
        mojom::ActionResultCode::kLoginNotLoginPage);
  }));
  ActResultFuture ext_res;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(2)),
                           MakeBundleClickRequest(base::Seconds(3))),
             ext_res.GetCallback());
  ExpectErrorResult(ext_res, mojom::ActionResultCode::kLoginNotLoginPage);
  EXPECT_THAT(observation_delays(),
              testing::ElementsAre(base::Seconds(1), base::Seconds(2)));

  base::RunLoop held_loop;
  fake_chrome_render_frame_.page_stability_monitor().HoldNextWait(
      held_loop.QuitClosure());
  ActResultFuture pause_res;
  task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(4)),
                           MakeBundleClickRequest(
                               base::Seconds(5),
                               /*supports_paint_stability=*/true,
                               mojom::ClickType::kLeftOnOccludedTarget,
                               /*requires_opening_web_contents=*/true)),
             pause_res.GetCallback());
  held_loop.Run();
  EXPECT_FALSE(task_->GetExecutionEngine().TabsCanOpenNewWebContents());
  task_->Pause(/*from_actor=*/false);
  ExpectErrorResult(pause_res, mojom::ActionResultCode::kTaskPaused);
}

TEST_F(ExecutionEngineStabilizeOncePerBundleTest,
       StandaloneSkipDelayAndEarlySettlePageStabilizationMetrics) {
  // A standalone page action records PageStabilization.
  {
    ActResultFuture res;
    task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1))),
               res.GetCallback());
    ExpectOkResult(res);
    histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 1);
  }

  // Page actions with skip-observation-delay do not form a coalesced run and
  // record PageStabilization.
  {
    auto skip1 = MakeBundleClickRequest(base::Seconds(1));
    skip1->SetSkipObservationDelay(true);
    auto skip2 = MakeBundleClickRequest(base::Seconds(2));
    skip2->SetSkipObservationDelay(true);
    ActResultFuture res;
    task_->Act(ToRequestList(std::move(skip1), std::move(skip2)),
               res.GetCallback());
    ExpectOkResult(res);
    histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 3);
  }

  // Navigation after Click1 triggers an early settle of its deferred wait, and
  // Click2 starts a new coalesced run with Click3. Click1 and Click2 defer and
  // aren't recorded; Click3 runs the shared wait and is recorded.
  {
    fake_chrome_render_frame_.SetNextToolHook(base::BindLambdaForTesting([&]() {
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://localhost/#section"), main_rfh())
          ->CommitSameDocument();
    }));
    ActResultFuture res;
    task_->Act(ToRequestList(MakeBundleClickRequest(base::Seconds(1)),
                             MakeBundleClickRequest(base::Seconds(2)),
                             MakeBundleClickRequest(base::Seconds(3))),
               res.GetCallback());
    ExpectOkResult(res);
    histograms_.ExpectTotalCount("Actor.Tools.PageStabilization.Click", 4);
  }
}

class ExecutionEngineNavigationGatingTest : public ExecutionEngineTest {
 public:
  ExecutionEngineNavigationGatingTest() {
    scoped_feature_list_.InitAndEnableFeature(kGlicCrossOriginNavigationGating);
  }
  ~ExecutionEngineNavigationGatingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExecutionEngineNavigationGatingTest,
       NavigationGatingMetricsRecordInitiatorOrigin_SameOriginAllowed) {
  const GURL kInitiatorUrl("https://initiator.com/");
  const url::Origin kInitiatorOrigin = url::Origin::Create(kInitiatorUrl);
  const GURL kDestinationUrl("https://destination.com/");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kDestinationUrl);

  content::MockNavigationHandle navigation_handle(kDestinationUrl, main_rfh());
  navigation_handle.set_initiator_origin(kInitiatorOrigin);

  base::test::TestFuture<MayActOnUrlBlockReason> future;
  task_->GetExecutionEngine().ShouldNavigationCommit(navigation_handle,
                                                     future.GetCallback());

  EXPECT_EQ(future.Get(), MayActOnUrlBlockReason::kAllowed);

  histograms_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision2",
      /*sample=*/ExecutionEngine::GatingDecision::kAllowSameOrigin,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample("Actor.NavigationGating.SameOriginSource",
                                 /*sample=*/true, /*expected_bucket_count=*/1);
  histograms_.ExpectUniqueSample("Actor.NavigationGating.SameSiteSource",
                                 /*sample=*/true, /*expected_bucket_count=*/1);
  histograms_.ExpectUniqueSample("Actor.NavigationGating.SameOriginInitiator",
                                 /*sample=*/false, /*expected_bucket_count=*/1);
  histograms_.ExpectUniqueSample("Actor.NavigationGating.SameSiteInitiator",
                                 /*sample=*/false, /*expected_bucket_count=*/1);
}

TEST_F(ExecutionEngineNavigationGatingTest,
       ShouldDeferNavigation_OpaqueSourceWithPrecursor) {
  const GURL kPrecursorUrl("https://example.com/");
  const GURL kDestinationUrl("https://example.com/other");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kPrecursorUrl);

  // Navigate to a data URL to get an opaque origin with the precursor.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("data:text/html,foo"), main_rfh());

  content::MockNavigationHandle navigation_handle(kDestinationUrl, main_rfh());

  base::test::TestFuture<MayActOnUrlBlockReason> future;
  task_->GetExecutionEngine().ShouldNavigationCommit(navigation_handle,
                                                     future.GetCallback());

  EXPECT_EQ(future.Get(), MayActOnUrlBlockReason::kAllowed);

  // Verify that SameOriginSource is true, indicating it used the precursor
  // origin.
  histograms_.ExpectUniqueSample("Actor.NavigationGating.SameOriginSource",
                                 /*sample=*/true, /*expected_bucket_count=*/1);
}

class ExecutionEngineUrlGatingTest : public ChromeRenderViewHostTestHarness {
 public:
  ExecutionEngineUrlGatingTest() = default;
  ~ExecutionEngineUrlGatingTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActor,
         optimization_guide::features::kOptimizationHints},
        {});

    ChromeRenderViewHostTestHarness::SetUp();

    mock_optimization_guide_keyed_service_ = static_cast<
        MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindOnce(
                    &ExecutionEngineUrlGatingTest::CreateOptimizationService)));

    // Simulate the component loading, as the implementation checks it, but the
    // actual outcomes are determined by the mock.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(
            {base::Version("123"),
             temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});

    ON_CALL(mock_actor_task_delegate_, RequestToShowUserConfirmationDialog)
        .WillByDefault([](TaskId, const url::Origin&, bool,
                          ActorTaskDelegate::UserConfirmationDialogCallback
                              callback) {
          std::move(callback).Run(
              webui::mojom::UserConfirmationDialogResponse::New(
                  webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                      false)));
        });
  }

  void TearDown() override {
    if (task_) {
      if (!task_->IsCompleted()) {
        task_->Stop(ActorTask::StoppedReason::kTabDetached);
      }
      task_.reset();
    }
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetExpectedOptimizationGuideCall(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision result) {
    EXPECT_CALL(
        *mock_optimization_guide_keyed_service_,
        CanApplyOptimization(
            url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
            testing::An<
                optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            result, optimization_guide::OptimizationMetadata{}));
  }

  ActorTask& GetTask() {
    GetExecutionEngine();
    CHECK(task_);
    return *task_;
  }

  void CheckUrl(const GURL& url,
                bool expected_allowed,
                EnterprisePolicyChecker::UrlBlockReason enterprise_reason) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);

    TestTabState tab_state(web_contents());

    task_policy_checker_.set_reason(enterprise_reason);

    WaitTool::SetNoDelayForTesting();
    std::unique_ptr<ToolRequest> tool_request = MakeWaitRequest(&tab_state.tab);
    ASSERT_TRUE(tool_request->RequiresUrlCheckInCurrentTab());
    ActResultFuture result;
    GetTask().Act(ToRequestList(tool_request), result.GetCallback());

    // The result should not be provided synchronously.
    EXPECT_FALSE(result.IsReady());
    ASSERT_EQ(result.Get().size(), 1u);
    EXPECT_THAT(result.Get()[0].tool_result.deprecated_action_result()->code,
                Conditional(expected_allowed, mojom::ActionResultCode::kOk,
                            Not(mojom::ActionResultCode::kOk)));
  }

  void CheckUrl(const GURL& url, bool expected_allowed) {
    return CheckUrl(url, expected_allowed,
                    EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
  }

  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;

  // Lazily creates a real ActorTask/ExecutionEngine and returns its engine, so
  // that IsAcceptableNavigationDestination tests can exercise the
  // OriginGatingChecker-backed path.
  ExecutionEngine& GetExecutionEngine(
      ExecutionEngine::AllowedSchemes allowed_schemes =
          ExecutionEngine::AllowedSchemes::kRequireHttpsOrHttp) {
    if (!task_) {
      std::unique_ptr<ui::UiEventDispatcher> task_dispatcher =
          ui::NewMockUiEventDispatcher();
      auto* mock =
          static_cast<ui::MockUiEventDispatcher*>(task_dispatcher.get());
      ON_CALL(*mock, OnPreTool)
          .WillByDefault(
              UiEventDispatcherCallback<ToolRequest>(base::BindRepeating(
                  MakeOkResult, /*requires_page_stabilization=*/true)));
      ON_CALL(*mock, OnPostTool)
          .WillByDefault(
              UiEventDispatcherCallback<ToolRequest>(base::BindRepeating(
                  MakeOkResult, /*requires_page_stabilization=*/true)));
      ON_CALL(*mock, OnActorTaskAsyncChange)
          .WillByDefault(UiEventDispatcherCallback<
                         ui::UiEventDispatcher::ActorTaskAsyncChange>(
              base::BindRepeating(MakeOkResult,
                                  /*requires_page_stabilization=*/true)));
      ActorTaskOptions options;
      options.delegate = mock_actor_task_delegate_.GetWeakPtr();
      options.allowed_schemes = allowed_schemes;
      task_ = ActorTask::CreateForTesting(*ActorKeyedService::Get(profile()),
                                          TaskId(1), std::move(task_dispatcher),
                                          TestTaskSourceInfo(),
                                          &task_policy_checker_, options);
    }
    return task_->GetExecutionEngine();
  }

  MockActorTaskDelegate& mock_actor_task_delegate() {
    return mock_actor_task_delegate_;
  }

 private:
  static std::unique_ptr<KeyedService> CreateOptimizationService(
      content::BrowserContext* context) {
    return std::make_unique<MockOptimizationGuideKeyedService>();
  }

  std::unique_ptr<ActorTask> task_;
  MockPolicyChecker task_policy_checker_{
      EnterprisePolicyChecker::UrlBlockReason::kNotBlocked};
  testing::NiceMock<MockActorTaskDelegate> mock_actor_task_delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_LocalhostPromptsForUserConfirmation \
  DISABLED_LocalhostPromptsForUserConfirmation
#else
#define MAYBE_LocalhostPromptsForUserConfirmation \
  LocalhostPromptsForUserConfirmation
#endif
TEST_F(ExecutionEngineUrlGatingTest,
       MAYBE_LocalhostPromptsForUserConfirmation) {
  const GURL localhost_url("http://localhost/");
  EXPECT_CALL(mock_actor_task_delegate(),
              RequestToShowUserConfirmationDialog(
                  _, url::Origin::Create(localhost_url), _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          webui::mojom::UserConfirmationDialogResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));
  CheckUrl(localhost_url, /*expected_allowed=*/true);

  const GURL ipv4_url("http://127.0.0.1/");
  EXPECT_CALL(mock_actor_task_delegate(),
              RequestToShowUserConfirmationDialog(
                  _, url::Origin::Create(ipv4_url), _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          webui::mojom::UserConfirmationDialogResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));
  CheckUrl(ipv4_url, /*expected_allowed=*/true);

  const GURL ipv6_url("http://[::1]/");
  EXPECT_CALL(mock_actor_task_delegate(),
              RequestToShowUserConfirmationDialog(
                  _, url::Origin::Create(ipv6_url), _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          webui::mojom::UserConfirmationDialogResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));
  CheckUrl(ipv6_url, /*expected_allowed=*/true);
}

TEST_F(ExecutionEngineUrlGatingTest, AllowAboutBlank) {
  CheckUrl(GURL(url::kAboutBlankURL), true);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_BlockIpAddress DISABLED_BlockIpAddress
#else
#define MAYBE_BlockIpAddress BlockIpAddress
#endif
TEST_F(ExecutionEngineUrlGatingTest, MAYBE_BlockIpAddress) {
  CheckUrl(GURL("https://8.8.8.8/"), false);
  CheckUrl(GURL("https://[2001:4860:4860::8888]/"), false);
}

TEST_F(ExecutionEngineUrlGatingTest, BlockNonHTTPScheme) {
  CheckUrl(GURL("file:///my_file"), false);
  CheckUrl(GURL("file://localhost/tmp"), false);
  CheckUrl(GURL(chrome::kChromeUIVersionURL), false);
}

TEST_F(ExecutionEngineUrlGatingTest, BlockInsecureHTTP) {
  CheckUrl(GURL("http://a.test/"), false);
}

TEST_F(ExecutionEngineUrlGatingTest, InsecureHTTPAllowedWhenSpecified) {
  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  GetExecutionEngine().IsAcceptableNavigationDestination(GURL("http://a.test/"),
                                                         allowed.GetCallback());
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kAllowed);
}

TEST_F(ExecutionEngineUrlGatingTest, AllowIfNotBlocked) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kTrue);
  CheckUrl(url, true);
}

TEST_F(ExecutionEngineUrlGatingTest, BlockIfInBlocklist) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kFalse);
  CheckUrl(url, false);
}

TEST_F(ExecutionEngineUrlGatingTest, AllowIfNotBlockedForOriginGating) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kTrue);
  base::test::TestFuture<bool> got_may_act;
  EXPECT_THAT(MaybeCheckOptimizationGuideForSensitiveUrl(
                  url, profile(), got_may_act.GetCallback()),
              base::test::HasValue());
  EXPECT_TRUE(got_may_act.Get());
}

TEST_F(ExecutionEngineUrlGatingTest, BlockIfInBlocklistForOriginGating) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kFalse);
  base::test::TestFuture<bool> got_may_act;
  EXPECT_THAT(MaybeCheckOptimizationGuideForSensitiveUrl(
                  url, profile(), got_may_act.GetCallback()),
              base::test::HasValue());
  EXPECT_FALSE(got_may_act.Get());
}

TEST_F(ExecutionEngineUrlGatingTest, AllowedOriginsFromNavigationGating) {
  const GURL url("https://c.test/");

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(2)
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          optimization_guide::OptimizationMetadata{}))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          optimization_guide::OptimizationMetadata{}));

  CheckUrl(url, false);

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);
    CheckUrl(url, true);
  }
}

TEST_F(ExecutionEngineUrlGatingTest, AllowIfDecisionUnknown) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kUnknown);
  CheckUrl(url, true);
}

TEST_F(ExecutionEngineUrlGatingTest, EnterprisePolicyBlock) {
  const GURL url("https://c.test/");
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  CheckUrl(url, false,
           EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked);
}

TEST_F(ExecutionEngineUrlGatingTest, EnterprisePolicyOrder) {
  const GURL https_blocked_url("https://c.test/");
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          https_blocked_url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  // Enterprise policy overrules the opt guide blocklist for a particular site.
  CheckUrl(https_blocked_url, true,
           EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);
  // Enterprise policy can't be used to bypass invariants like supported
  // schemes.
  CheckUrl(GURL("file:///my_file"), false,
           EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);
}

TEST_F(ExecutionEngineUrlGatingTest,
       IsAcceptableNavigationDestination_AllowedByCache) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  ExecutionEngine& engine = GetExecutionEngine();
  engine.GetOriginGatingChecker().AllowNavigationTo(url::Origin::Create(url),
                                                    /*is_user_confirmed=*/true);
  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  engine.IsAcceptableNavigationDestination(url, allowed.GetCallback());
  // Allowed by cache.
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kAllowed);
}

TEST_F(ExecutionEngineUrlGatingTest,
       IsAcceptableNavigationDestination_FailsOpen) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  GetExecutionEngine().IsAcceptableNavigationDestination(url,
                                                         allowed.GetCallback());
  // Not allowed by the cache, but the policy fails open (without consulting
  // the sensitive sites list).
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kAllowed);
}

TEST_F(ExecutionEngineUrlGatingTest,
       IsAcceptableNavigationDestination_DefaultRejectsNewTabPage) {
  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  GetExecutionEngine().IsAcceptableNavigationDestination(
      GURL("chrome://newtab/"), allowed.GetCallback());
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kWrongScheme);
}

TEST_F(ExecutionEngineUrlGatingTest,
       IsAcceptableNavigationDestination_TtcAllowsNewTabPage) {
  base::test::TestFuture<MayActOnUrlBlockReason> allowed_ntp;
  GetExecutionEngine(ExecutionEngine::AllowedSchemes::kRequireHttpsOrHttpOrNtp)
      .IsAcceptableNavigationDestination(GURL("chrome://newtab/"),
                                         allowed_ntp.GetCallback());
  EXPECT_EQ(allowed_ntp.Get(), MayActOnUrlBlockReason::kAllowed);

  // Still rejects non-NTP chrome:// URLs.
  base::test::TestFuture<MayActOnUrlBlockReason> allowed_settings;
  GetExecutionEngine(ExecutionEngine::AllowedSchemes::kRequireHttpsOrHttpOrNtp)
      .IsAcceptableNavigationDestination(GURL("chrome://settings/"),
                                         allowed_settings.GetCallback());
  EXPECT_EQ(allowed_settings.Get(), MayActOnUrlBlockReason::kWrongScheme);
}

TEST_F(ExecutionEngineUrlGatingTest, SafetyChecksForNextAction_AllowedByCache) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  GetExecutionEngine().GetOriginGatingChecker().AllowNavigationTo(
      url::Origin::Create(url), /*is_user_confirmed=*/true);
  CheckUrl(url, /*expected_allowed=*/true);
}

TEST_F(ExecutionEngineUrlGatingTest,
       SafetyChecksForNextAction_PromptsForSensitiveSite) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kFalse);

  EXPECT_CALL(
      mock_actor_task_delegate(),
      RequestToShowUserConfirmationDialog(_, url::Origin::Create(url),
                                          /*for_blocklisted_origin=*/true, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          webui::mojom::UserConfirmationDialogResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));

  CheckUrl(url, /*expected_allowed=*/true);

  // Subsequent check for the same origin is allowed via cache without
  // re-prompting.
  EXPECT_CALL(mock_actor_task_delegate(),
              RequestToShowUserConfirmationDialog(_, _, _, _))
      .Times(0);
  CheckUrl(url, /*expected_allowed=*/true);

  // Subsequent check for a same-site subdomain is also allowed via cache
  // without re-prompting.
  const GURL sub_url("https://sub.c.test/");
  CheckUrl(sub_url, /*expected_allowed=*/true);
}

TEST_F(ExecutionEngineUrlGatingTest,
       SafetyChecksForNextAction_PromptsForSensitiveSiteFromCommandLine) {
  const GURL url("https://c.test/");
  const GURL sub_url("https://sub.c.test/");
  const GURL other_sub_url("https://sub.d.test/");
  const GURL non_matching_url("https://other.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kActorSensitiveSites, "https://c.test,https://d.test");

  // Matching URL prompts for confirmation without querying Optimization Guide.
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  EXPECT_CALL(
      mock_actor_task_delegate(),
      RequestToShowUserConfirmationDialog(_, url::Origin::Create(url),
                                          /*for_blocklisted_origin=*/true, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          webui::mojom::UserConfirmationDialogResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));

  CheckUrl(url, /*expected_allowed=*/true);

  // Subsequent check for the same origin is allowed via cache.
  EXPECT_CALL(mock_actor_task_delegate(),
              RequestToShowUserConfirmationDialog(_, _, _, _))
      .Times(0);
  CheckUrl(url, /*expected_allowed=*/true);

  // Same-site subdomain is also allowed via cache without re-prompting.
  CheckUrl(sub_url, /*expected_allowed=*/true);

  // Same-site subdomain of an unconfirmed sensitive site matches SchemefulSite
  // and prompts without Optimization Guide.
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          other_sub_url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  EXPECT_CALL(
      mock_actor_task_delegate(),
      RequestToShowUserConfirmationDialog(_, url::Origin::Create(other_sub_url),
                                          /*for_blocklisted_origin=*/true, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          webui::mojom::UserConfirmationDialogResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));
  CheckUrl(other_sub_url, /*expected_allowed=*/true);

  // Non-matching URL falls back to Optimization Guide.
  SetExpectedOptimizationGuideCall(
      non_matching_url, optimization_guide::OptimizationGuideDecision::kTrue);
  CheckUrl(non_matching_url, /*expected_allowed=*/true);
}

TEST_F(ExecutionEngineUrlGatingTest,
       ShouldDeferNavigation_CrossOriginNonSensitiveAllowed) {
  const GURL source_url("https://a.test/");
  const GURL destination_url("https://b.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  SetExpectedOptimizationGuideCall(
      destination_url, optimization_guide::OptimizationGuideDecision::kTrue);
  EXPECT_CALL(
      mock_actor_task_delegate(),
      RequestToConfirmNavigation(_, url::Origin::Create(destination_url), _))
      .WillOnce(base::test::RunOnceCallback<2>(
          webui::mojom::NavigationConfirmationResponse::New(
              webui::mojom::ConfirmationRequestResult::NewPermissionGranted(
                  true))));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             source_url);

  content::MockNavigationHandle navigation_handle(destination_url, main_rfh());

  base::test::TestFuture<MayActOnUrlBlockReason> future;
  GetExecutionEngine().ShouldNavigationCommit(navigation_handle,
                                              future.GetCallback());

  EXPECT_EQ(future.Get(), MayActOnUrlBlockReason::kAllowed);
}

TEST_F(ExecutionEngineUrlGatingTest,
       ShouldDeferNavigation_CrossOriginSensitiveBlocked) {
  const GURL source_url("https://a.test/");
  const GURL destination_url("https://b.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicCrossOriginNavigationGating);

  SetExpectedOptimizationGuideCall(
      destination_url, optimization_guide::OptimizationGuideDecision::kFalse);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             source_url);

  content::MockNavigationHandle navigation_handle(destination_url, main_rfh());

  base::test::TestFuture<MayActOnUrlBlockReason> future;
  GetExecutionEngine().ShouldNavigationCommit(navigation_handle,
                                              future.GetCallback());

  EXPECT_EQ(future.Get(), MayActOnUrlBlockReason::kOptimizationGuideBlock);
}

struct MimeTestCase {
  std::optional<std::string_view> content_type_header;
  bool expected_allowed;
};

class ExecutionEngineMimeGatingTest
    : public ExecutionEngineUrlGatingTest,
      public testing::WithParamInterface<MimeTestCase> {
 public:
  std::optional<std::string_view> content_type_header() const {
    return GetParam().content_type_header;
  }

  MayActOnUrlBlockReason expected_reason() const {
    return GetParam().expected_allowed
               ? MayActOnUrlBlockReason::kAllowed
               : MayActOnUrlBlockReason::kDangerousMimeType;
  }
};

TEST_P(ExecutionEngineMimeGatingTest, HandlesMimeTypes) {
  const GURL source_url("https://example.com/");
  const GURL destination_url("https://example.com/api");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kGlicCrossOriginNavigationGating,
                            kGlicBlockNavigationToDangerousContentTypes},
      /*disabled_features=*/{});

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             source_url);

  content::MockNavigationHandle navigation_handle(destination_url, main_rfh());
  navigation_handle.set_initiator_origin(url::Origin::Create(source_url));

  net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1), "200 OK");
  if (content_type_header().has_value()) {
    builder.AddHeader("Content-Type", *content_type_header());
  }
  navigation_handle.set_response_headers(builder.Build());

  base::test::TestFuture<MayActOnUrlBlockReason> future;
  GetExecutionEngine().ShouldNavigationCommit(navigation_handle,
                                              future.GetCallback());

  EXPECT_EQ(future.Get(), expected_reason());
}

INSTANTIATE_TEST_SUITE_P(,
                         ExecutionEngineMimeGatingTest,
                         testing::ValuesIn(std::initializer_list<MimeTestCase>{
                             {"application/json", false},
                             {"application/ld+json", false},
                             {"application/x-javascript", false},
                             {"application/hal+json", false},
                             {"application/xml", false},
                             {"text/csv", false},
                             {"text/comma-separated-values", false},
                             {"text/tsv", false},
                             {"text/tab-separated-values", false},
                             {"text/plain", true},
                             {std::nullopt, true},
                             {"text/html", true},
                         }),
                         [](const testing::TestParamInfo<MimeTestCase>& info) {
                           std::string mime_type(
                               info.param.content_type_header.value_or("null"));
                           std::ranges::replace(mime_type, '/', '_');
                           std::ranges::replace(mime_type, '+', '_');
                           std::ranges::replace(mime_type, '-', '_');
                           return mime_type;
                         });

TEST_F(ExecutionEngineUrlGatingTest,
       ShouldDeferNavigation_DangerousMimeTypeFeatureDisabled) {
  const GURL source_url("https://a.test/");
  const GURL destination_url("https://a.test/data.json");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kGlicCrossOriginNavigationGating},
      /*disabled_features=*/{kGlicBlockNavigationToDangerousContentTypes});

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             source_url);

  content::MockNavigationHandle navigation_handle(destination_url, main_rfh());
  net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1), "200 OK");
  builder.AddHeader("Content-Type", "application/json");
  navigation_handle.set_response_headers(builder.Build());

  base::test::TestFuture<MayActOnUrlBlockReason> future;
  GetExecutionEngine().ShouldNavigationCommit(navigation_handle,
                                              future.GetCallback());

  EXPECT_EQ(future.Get(), MayActOnUrlBlockReason::kAllowed);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
mojom::ExecuteJsResponsePtr MakeForgedExecuteJsResponse() {
  return mojom::ExecuteJsResponse::New(
      /*result=*/"bad\xFF\xFEutf8" + std::string(kExecuteJsMaxResultBytes, 'r'),
      /*truncated=*/false,
      /*js_error_message=*/"bad\xFF\xFEmsg",
      /*console_logs=*/"bad\xFF\xFElog" +
          std::string(kExecuteJsMaxConsoleBytes, 'c'));
}

class ForgedExecuteJsResponseRenderFrame : public FakeChromeRenderFrame {
 public:
  void set_fail_initialize_tool(bool fail) { fail_initialize_tool_ = fail; }
  void set_populate_script_tool_response(bool populate) {
    populate_script_tool_response_ = populate;
  }

  int initialize_tool_calls() const { return initialize_tool_calls_; }
  int execute_tool_calls() const { return execute_tool_calls_; }
  int invoke_tool_calls() const { return invoke_tool_calls_; }

  void InitializeTool(actor::mojom::ToolInvocationPtr request,
                      InitializeToolCallback callback) override {
    ++initialize_tool_calls_;
    if (fail_initialize_tool_) {
      mojom::ActionResultPtr error =
          MakeResult(mojom::ActionResultCode::kInvalidDomNodeId,
                     /*requires_page_stabilization=*/false, "Target not found");
      error->execute_js_response = MakeForgedExecuteJsResponse();
      std::move(callback).Run(
          mojom::InitializeToolResult::NewErrorResult(std::move(error)));
      return;
    }
    FakeChromeRenderFrame::InitializeTool(std::move(request),
                                          std::move(callback));
  }

  void ExecuteTool(const actor::TaskId& task_id,
                   ExecuteToolCallback callback) override {
    ++execute_tool_calls_;
    mojom::ActionResultPtr result = MakeOkResult();
    result->execute_js_response = MakeForgedExecuteJsResponse();
    std::move(callback).Run(std::move(result));
  }

  void InvokeTool(actor::mojom::ToolInvocationPtr request,
                  InvokeToolCallback callback) override {
    ++invoke_tool_calls_;
    mojom::ActionResultPtr result = MakeOkResult();
    if (populate_script_tool_response_) {
      result->script_tool_response = mojom::ScriptToolResponse::New();
      result->script_tool_response->tool = blink::mojom::ScriptTool::New();
      result->script_tool_response->tool->name = "test_tool";
      result->script_tool_response->result = "ok";
    }
    result->execute_js_response = MakeForgedExecuteJsResponse();
    std::move(callback).Run(std::move(result));
  }

 private:
  bool fail_initialize_tool_ = false;
  bool populate_script_tool_response_ = false;
  int initialize_tool_calls_ = 0;
  int execute_tool_calls_ = 0;
  int invoke_tool_calls_ = 0;
};

TEST_F(ExecutionEngineTest, PageToolInvokeStripsForgedExecuteJsResponse) {
  base::test::ScopedFeatureList disable_split_mode;
  disable_split_mode.InitAndDisableFeature(
      features::kGlicActorSplitValidateAndExecute);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ForgedExecuteJsResponseRenderFrame fake_render_frame;
  fake_render_frame.OverrideBinder(main_rfh());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());

  const auto& action_results = result.Get();
  EXPECT_EQ(fake_render_frame.initialize_tool_calls(), 0);
  EXPECT_EQ(fake_render_frame.execute_tool_calls(), 0);
  EXPECT_EQ(fake_render_frame.invoke_tool_calls(), 1);
  ASSERT_EQ(action_results.size(), 1u);
  const mojom::ActionResultPtr& action_result =
      action_results[0].tool_result.deprecated_action_result();
  ASSERT_TRUE(action_result);
  EXPECT_TRUE(IsOk(*action_result));
  EXPECT_FALSE(action_result->execute_js_response);

  optimization_guide::proto::ActionsResult actions_result;
  CopyExecuteJsResults(actions_result, action_results);
  EXPECT_EQ(actions_result.execute_js_results_size(), 0);
}

TEST_F(ExecutionEngineTest,
       PageToolSplitModeExecuteStripsForgedExecuteJsResponse) {
  base::test::ScopedFeatureList split_features;
  split_features.InitWithFeatures(
      /*enabled_features=*/{features::kGlicActorSplitValidateAndExecute,
                            features::kGlicActorUiMagicCursor},
      /*disabled_features=*/{});

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ForgedExecuteJsResponseRenderFrame fake_render_frame;
  fake_render_frame.OverrideBinder(main_rfh());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());

  const auto& action_results = result.Get();
  EXPECT_EQ(fake_render_frame.initialize_tool_calls(), 1);
  EXPECT_EQ(fake_render_frame.execute_tool_calls(), 1);
  EXPECT_EQ(fake_render_frame.invoke_tool_calls(), 0);
  ASSERT_EQ(action_results.size(), 1u);
  const mojom::ActionResultPtr& action_result =
      action_results[0].tool_result.deprecated_action_result();
  ASSERT_TRUE(action_result);
  EXPECT_TRUE(IsOk(*action_result));
  EXPECT_FALSE(action_result->execute_js_response);

  optimization_guide::proto::ActionsResult actions_result;
  CopyExecuteJsResults(actions_result, action_results);
  EXPECT_EQ(actions_result.execute_js_results_size(), 0);
}

TEST_F(ExecutionEngineTest,
       PageToolSplitModeInitializeErrorStripsForgedExecuteJsResponse) {
  base::test::ScopedFeatureList split_features;
  split_features.InitWithFeatures(
      /*enabled_features=*/{features::kGlicActorSplitValidateAndExecute,
                            features::kGlicActorUiMagicCursor},
      /*disabled_features=*/{});

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ForgedExecuteJsResponseRenderFrame fake_render_frame;
  fake_render_frame.set_fail_initialize_tool(true);
  fake_render_frame.OverrideBinder(main_rfh());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(action), result.GetCallback());

  const auto& action_results = result.Get();
  EXPECT_EQ(fake_render_frame.initialize_tool_calls(), 1);
  EXPECT_EQ(fake_render_frame.execute_tool_calls(), 0);
  EXPECT_EQ(fake_render_frame.invoke_tool_calls(), 0);
  ASSERT_EQ(action_results.size(), 1u);
  const mojom::ActionResultPtr& action_result =
      action_results[0].tool_result.deprecated_action_result();
  ASSERT_TRUE(action_result);
  EXPECT_EQ(action_result->code, mojom::ActionResultCode::kInvalidDomNodeId);
  EXPECT_FALSE(action_result->execute_js_response);

  optimization_guide::proto::ActionsResult actions_result;
  CopyExecuteJsResults(actions_result, action_results);
  EXPECT_EQ(actions_result.execute_js_results_size(), 0);
}

TEST_F(ExecutionEngineTest, ScriptToolHostStripsForgedExecuteJsResponse) {
  base::test::ScopedFeatureList web_mcp_features;
  web_mcp_features.InitAndEnableFeature(blink::features::kWebMCP);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  ForgedExecuteJsResponseRenderFrame fake_render_frame;
  fake_render_frame.set_populate_script_tool_response(true);
  fake_render_frame.OverrideBinder(main_rfh());

  const base::UnguessableToken document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(main_rfh())
              ->token();
  std::unique_ptr<ToolRequest> action = std::make_unique<ScriptToolRequest>(
      ActorSurfaceHandle::From(GetTab()->GetHandle()), document_id, "test_tool",
      "{}");

  ActResultFuture result;
  task_->Act(ToRequestList(action), result.GetCallback());

  const auto& action_results = result.Get();
  EXPECT_EQ(fake_render_frame.initialize_tool_calls(), 0);
  EXPECT_EQ(fake_render_frame.execute_tool_calls(), 0);
  EXPECT_EQ(fake_render_frame.invoke_tool_calls(), 1);
  ASSERT_EQ(action_results.size(), 1u);
  const mojom::ActionResultPtr& action_result =
      action_results[0].tool_result.deprecated_action_result();
  ASSERT_TRUE(action_result);
  EXPECT_TRUE(IsOk(*action_result));
  ASSERT_TRUE(action_result->script_tool_response);
  EXPECT_EQ(action_result->script_tool_response->result, "ok");
  EXPECT_FALSE(action_result->execute_js_response);

  optimization_guide::proto::ActionsResult actions_result;
  CopyExecuteJsResults(actions_result, action_results);
  EXPECT_EQ(actions_result.execute_js_results_size(), 0);
}
#endif  // !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)

}  // namespace

}  // namespace actor
