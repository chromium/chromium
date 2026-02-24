// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <optional>

#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/autofill_selection_dialog_event_handler.h"
#include "chrome/browser/actor/enterprise_policy_url_checker.h"
#include "chrome/browser/actor/safety_list_manager.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tool_request_variant.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/fake_tool.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace actor {

using ::optimization_guide::proto::Actions;
using testing::_;
using testing::Eq;
using testing::Field;
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
      void,
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
      void,
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
    std::move(callback).Run(MakeOkResult());
  }
  void InvokeTool(actor::mojom::ToolInvocationPtr request,
                  InvokeToolCallback callback) override {
    std::move(callback).Run(MakeOkResult());
  }
  void StartActorJournal(
      mojo::PendingAssociatedRemote<actor::mojom::JournalClient> client)
      override {}
  void CreatePageStabilityMonitor(
      mojo::PendingReceiver<actor::mojom::PageStabilityMonitor> monitor,
      const TaskId& task_id,
      bool supports_paint_stability) override {}
  void CancelTool(const TaskId& task_id) override {}
  void GetCrossDocumentScriptToolResult(
      GetCrossDocumentScriptToolResultCallback callback) override {
    std::move(callback).Run("");
  }
#if BUILDFLAG(IS_ANDROID)
  void SetCCTClientHeader(const std::string& header) override {}
#endif

 private:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>(
                  std::move(handle)));
  }

  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;
};

class MockActorTaskDelegate : public ActorTaskDelegate {
 public:
  MockActorTaskDelegate() = default;
  ~MockActorTaskDelegate() override = default;

  MOCK_METHOD(void,
              OnTabAddedToTask,
              (TaskId task_id, const tabs::TabInterface::Handle& tab_handle),
              (override));

  MOCK_METHOD(void,
              RequestToShowCredentialSelectionDialog,
              (TaskId task_id,
               (const base::flat_map<std::string, gfx::Image>&)icons,
               const std::vector<actor_login::Credential>& credentials,
               CredentialSelectedCallback callback),
              (override));

  MOCK_METHOD(void,
              RequestToShowUserConfirmationDialog,
              (TaskId task_id,
               const url::Origin& destination,
               bool for_blocklisted_origin,
               UserConfirmationDialogCallback callback),
              (override));

  MOCK_METHOD(void,
              RequestToConfirmNavigation,
              (TaskId task_id,
               const url::Origin& destination,
               NavigationConfirmationCallback callback),
              (override));

  MOCK_METHOD(void,
              RequestToShowAutofillSuggestionsDialog,
              (actor::TaskId task_id,
               std::vector<autofill::ActorFormFillingRequest> requests,
               base::WeakPtr<AutofillSelectionDialogEventHandler> handler,
               AutofillSuggestionSelectedCallback callback),
              (override));

  base::WeakPtr<MockActorTaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockActorTaskDelegate> weak_factory_{this};
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

    // ExecutionEngine & ActorTask use separate UiEventDispatcher objects, so
    // we create separate mocks for each.
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher =
        ui::NewMockUiEventDispatcher();
    std::unique_ptr<ui::UiEventDispatcher> task_ui_event_dispatcher =
        ui::NewMockUiEventDispatcher();
    mock_ui_event_dispatcher_ =
        static_cast<ui::MockUiEventDispatcher*>(ui_event_dispatcher.get());
    task_mock_ui_event_dispatcher_ =
        static_cast<ui::MockUiEventDispatcher*>(task_ui_event_dispatcher.get());

    ScopedExecutionEngineFactory scoped_execution_engine_factory(
        base::BindLambdaForTesting([&](actor::ActorTask& task) {
          CHECK(ui_event_dispatcher);
          return actor::ExecutionEngine::CreateForTesting(
              task, std::move(ui_event_dispatcher));
        }));

    task_ = ActorTask::CreateForTesting(
        *ActorKeyedService::Get(profile()), TaskId(1),
        std::move(task_ui_event_dispatcher),
        /*options=*/nullptr, &no_enterprise_checker_,
        mock_actor_task_delegate_.GetWeakPtr());

    for (auto& mock :
         {mock_ui_event_dispatcher_, task_mock_ui_event_dispatcher_}) {
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
    }
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(mock_ui_event_dispatcher_);
    testing::Mock::VerifyAndClearExpectations(task_mock_ui_event_dispatcher_);
    mock_ui_event_dispatcher_ = nullptr;
    task_mock_ui_event_dispatcher_ = nullptr;
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
              GetTab()->GetHandle(), target, mojom::ClickType::kLeft,
              mojom::ClickCount::kSingle);
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
    return IsOk(*success.Get<0>());
  }

  tabs::MockTabInterface* GetTab() {
    return tab_state_ ? &tab_state_->tab : nullptr;
  }

  void AssociateTabInterface() { tab_state_.emplace(web_contents()); }
  void ClearTabInterface() { tab_state_.reset(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;
  FakeChromeRenderFrame fake_chrome_render_frame_;
  std::unique_ptr<ActorTask> task_;
  raw_ptr<ui::MockUiEventDispatcher> mock_ui_event_dispatcher_;
  raw_ptr<ui::MockUiEventDispatcher> task_mock_ui_event_dispatcher_;
  testing::NiceMock<MockActorTaskDelegate> mock_actor_task_delegate_;

 private:
  std::optional<TestTabState> tab_state_;

  MockPolicyChecker no_enterprise_checker_{
      EnterprisePolicyBlockReason::kNotBlocked};
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
      *task_mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<ChangeTaskState>(AllOf(
          Field(&ChangeTaskState::old_state, ActorTask::State::kCreated),
          Field(&ChangeTaskState::new_state, ActorTask::State::kActing)))))
      .Times(1);
  EXPECT_CALL(
      *task_mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<ChangeTaskState>(AllOf(
          Field(&ChangeTaskState::old_state, ActorTask::State::kActing),
          Field(&ChangeTaskState::new_state, ActorTask::State::kReflecting)))));
  EXPECT_CALL(*task_mock_ui_event_dispatcher_,
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
  EXPECT_CALL(*task_mock_ui_event_dispatcher_,
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

  base::test::TestFuture<ToolCallback> on_invoke_future;
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

  EXPECT_CALL(*task_mock_ui_event_dispatcher_,
              OnActorTaskSyncChange(
                  VariantWith<ui::MockUiEventDispatcher::RemoveTab>(_)))
      .Times(testing::AnyNumber());
  EXPECT_CALL(
      *task_mock_ui_event_dispatcher_,
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

  base::test::TestFuture<ToolCallback> on_invoke_future;

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
  EXPECT_TRUE(IsOk(*result.Get<0>()));

  auto& actions_result = result.Get<2>();
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
  task_->AddTab(GetTab()->GetHandle(), base::DoNothing());
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
  task_->AddTab(GetTab()->GetHandle(), base::DoNothing());
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
  task_->AddTab(GetTab()->GetHandle(), base::DoNothing());
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
  task_->AddTab(GetTab()->GetHandle(), base::DoNothing());
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
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS;

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
  EXPECT_EQ(
      received_requests[0].requested_data,
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  EXPECT_EQ(received_handler.get(), &event_handler);
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

  EXPECT_EQ(task_->GetExecutionEngine().ShouldDeferNavigation(
                navigation_handle, base::NullCallback()),
            content::NavigationThrottle::PROCEED);

  histograms_.ExpectUniqueSample(
      "Actor.NavigationGating.GatingDecision",
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

}  // namespace

}  // namespace actor
