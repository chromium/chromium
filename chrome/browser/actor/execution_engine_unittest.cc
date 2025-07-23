// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <optional>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/mock_event_dispatcher.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace actor {

using ::optimization_guide::proto::Actions;
using testing::_;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::Property;
using testing::VariantWith;
using ChangeTaskState = ui::UiEventDispatcher::ChangeTaskState;
using AddTab = ui::UiEventDispatcher::AddTab;

namespace {
constexpr int kFakeContentNodeId = 123;
constexpr char kActionResultHistogram[] =
    "Actor.ExecutionEngine.Action.ResultCode";

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
  void SetSupportsDraggableRegions(bool supports_draggable_regions) override {}
  void SetShouldDeferMediaLoad(bool should_defer) override {}

  void InvokeTool(actor::mojom::ToolInvocationPtr request,
                  InvokeToolCallback callback) override {
    std::move(callback).Run(MakeOkResult());
  }
  void StartActorJournal(
      mojo::PendingAssociatedRemote<actor::mojom::JournalClient> client)
      override {}

 private:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>(
                  std::move(handle)));
  }

  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;
};

class ExecutionEngineTest : public ChromeRenderViewHostTestHarness {
 public:
  ExecutionEngineTest() = default;
  ~ExecutionEngineTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor},
        /*disabled_features=*/{});
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

    auto execution_engine = ExecutionEngine::CreateForTesting(
        profile(), std::move(ui_event_dispatcher));
    auto raw_execution_engine = execution_engine.get();
    task_ = std::make_unique<ActorTask>(profile(), std::move(execution_engine),
                                        std::move(task_ui_event_dispatcher));
    task_->SetIdForTesting(0);
    raw_execution_engine->SetOwner(task_.get());

    for (auto& mock :
         {mock_ui_event_dispatcher_, task_mock_ui_event_dispatcher_}) {
      ON_CALL(*mock, OnPreFirstAct(_, _))
          .WillByDefault(Invoke(Invoke(
              UiEventDispatcherCallback<ui::UiEventDispatcher::FirstActInfo>(
                  base::BindRepeating(MakeOkResult)))));
      ON_CALL(*mock, OnPreTool(_, _))
          .WillByDefault(Invoke(UiEventDispatcherCallback<ToolRequest>(
              base::BindRepeating(MakeOkResult))));
      ON_CALL(*mock, OnPostTool(_, _))
          .WillByDefault(Invoke(UiEventDispatcherCallback<ToolRequest>(
              base::BindRepeating(MakeOkResult))));
      ON_CALL(*mock, OnActorTaskAsyncChange(_, _))
          .WillByDefault(Invoke(UiEventDispatcherCallback<
                                ui::UiEventDispatcher::ActorTaskAsyncChange>(
              base::BindRepeating(MakeOkResult))));
    }
  }

  void TearDown() override {
    mock_ui_event_dispatcher_ = nullptr;
    task_mock_ui_event_dispatcher_ = nullptr;
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
              GetTab()->GetHandle(), target, MouseClickType::kLeft,
              MouseClickCount::kSingle);
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

    base::test::TestFuture<mojom::ActionResultPtr, std::optional<size_t>>
        success;
    std::unique_ptr<ToolRequest> action = std::move(make_action).Run();
    task_->Act(ToRequestList(std::move(action)), success.GetCallback());
    return IsOk(*success.Get<0>());
  }

  tabs::MockTabInterface* GetTab() {
    return tab_state_ ? &tab_state_->tab : nullptr;
  }

  void AssociateTabInterface() { tab_state_.emplace(web_contents()); }
  void ClearTabInterface() { tab_state_.reset(); }

  base::HistogramTester histograms_;
  FakeChromeRenderFrame fake_chrome_render_frame_;
  std::unique_ptr<ActorTask> task_;
  raw_ptr<ui::MockUiEventDispatcher> mock_ui_event_dispatcher_;
  raw_ptr<ui::MockUiEventDispatcher> task_mock_ui_event_dispatcher_;

 private:
  struct TabState {
    explicit TabState(content::WebContents* web_contents) {
      ON_CALL(tab, GetContents).WillByDefault(::testing::Return(web_contents));
      ON_CALL(tab, RegisterWillDetach)
          .WillByDefault([this](tabs::TabInterface::WillDetach callback) {
            return will_detach_callback_list_.Add(std::move(callback));
          });
    }

    ~TabState() {
      will_detach_callback_list_.Notify(
          &tab, tabs::TabInterface::DetachReason::kDelete);
    }

    using WillDetachCallbackList =
        base::RepeatingCallbackList<void(tabs::TabInterface*,
                                         tabs::TabInterface::DetachReason)>;
    WillDetachCallbackList will_detach_callback_list_;

    tabs::MockTabInterface tab;
  };
  std::optional<TabState> tab_state_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExecutionEngineTest, ActSucceedsOnSupportedUrl) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreFirstAct(_, _)).Times(1);
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
}

TEST_F(ExecutionEngineTest, ActFailsOnUnsupportedUrl) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreFirstAct(_, _)).Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool(_, _)).Times(0);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool(_, _)).Times(0);
  EXPECT_FALSE(Act(GURL(chrome::kChromeUIVersionURL),
                   MakeClickCallback(kFakeContentNodeId)));
}

TEST_F(ExecutionEngineTest, UiOnPreFirstActFails) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreFirstAct(_, _))
      .WillOnce(
          Invoke(UiEventDispatcherCallback<ui::UiEventDispatcher::FirstActInfo>(
              base::BindRepeating(MakeErrorResult))));
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool(_, _)).Times(0);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool(_, _)).Times(0);
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kError, 1);
}

TEST_F(ExecutionEngineTest, UiOnPreToolFails) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreFirstAct(_, _)).Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool(_, _))
      .WillOnce(Invoke(UiEventDispatcherCallback<ToolRequest>(
          base::BindRepeating(MakeErrorResult))));
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool(_, _)).Times(0);
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kError, 1);
}

TEST_F(ExecutionEngineTest, UiOnPostToolFails) {
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreFirstAct(_, _)).Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPreTool(_, _)).Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_, OnPostTool(_, _))
      .WillOnce(Invoke(UiEventDispatcherCallback<ToolRequest>(
          base::BindRepeating(MakeErrorResult))));
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kError, 1);
}

TEST_F(ExecutionEngineTest, ActFailsWhenAddTabFails) {
  EXPECT_CALL(*task_mock_ui_event_dispatcher_,
              OnActorTaskAsyncChange(VariantWith<AddTab>(_), _))
      .WillOnce(Invoke(UiEventDispatcherCallback<
                       ui::UiEventDispatcher::ActorTaskAsyncChange>(
          base::BindRepeating(MakeErrorResult))));
  EXPECT_FALSE(
      Act(GURL("http://localhost/"), MakeClickCallback(kFakeContentNodeId)));
  histograms_.ExpectUniqueSample(kActionResultHistogram,
                                 mojom::ActionResultCode::kError, 1);
}

TEST_F(ExecutionEngineTest, ActFailsWhenTabDestroyed) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  base::test::TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;

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

TEST_F(ExecutionEngineTest, CrossOriginNavigationBeforeAction) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  base::test::TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  auto execution_engine = std::make_unique<ExecutionEngine>(profile());
  ActorTask task(profile(), std::move(execution_engine),
                 ui::NewMockUiEventDispatcher());
  std::unique_ptr<ToolRequest> action =
      MakeClickCallback(kFakeContentNodeId).Run();
  task_->Act(ToRequestList(std::move(action)), result.GetCallback());

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

}  // namespace

}  // namespace actor
