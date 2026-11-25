// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"  // nogncheck
#else
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#endif

namespace context_hub {
namespace {

using browser::context_hub::mojom::SkillRecordingActionType;

class TabActiveObserverTest : public testing::Test {
 public:
  TabActiveObserverTest() = default;
  TabActiveObserverTest(const TabActiveObserverTest&) = delete;
  TabActiveObserverTest& operator=(const TabActiveObserverTest&) = delete;
  ~TabActiveObserverTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactory(&profile_,
                            ProfileKeyedServiceFactory::TestingFactory());
#else
    SpeechRecognitionServiceFactory::GetInstanceForTest()->SetTestingFactory(
        &profile_, ProfileKeyedServiceFactory::TestingFactory());
#endif
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &profile_, content::SiteInstance::Create(&profile_));
    service_.StartRecording();
    observer_ =
        std::make_unique<TabActiveObserver>(&service_, web_contents_.get());
  }

  void TearDown() override {
    observer_.reset();
    ax_tree_managers_.clear();
    web_contents_.reset();
  }

 protected:
  void SetDefaultClickableAXTreeForFrame(
      content::RenderFrameHost* rfh,
      const std::string& button_name = "Submit") {
    ui::AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;
    root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
    root.child_ids = {2};

    ui::AXNodeData button;
    button.id = 2;
    button.role = ax::mojom::Role::kButton;
    button.SetNameChecked(button_name);
    button.relative_bounds.bounds = gfx::RectF(0, 0, 300, 300);

    const ui::AXTreeID tree_id = rfh->GetAXTreeID();
    ui::AXTreeUpdate update;
    update.has_tree_data = true;
    update.tree_data.tree_id = tree_id;
    update.root_id = 1;
    update.nodes = {root, button};
    ax_tree_managers_.erase(tree_id);
    ax_tree_managers_.emplace(tree_id,
                              std::make_unique<ui::AXTreeManager>(
                                  std::make_unique<ui::AXTree>(update)));
  }

  void PrepareMainFrameForInput() {
    content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();
    // Flush post-navigation paint-holding so `ForwardMouseEvent()` dispatches
    // immediately in unit tests without a real compositor frame.
    web_contents_->WasHidden();
    web_contents_->WasShown();
  }

  void NavigateTo(const GURL& url) {
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(url);
    PrepareMainFrameForInput();
    SetDefaultClickableAXTreeForFrame(web_contents_->GetPrimaryMainFrame());
  }

  void SimulateMouseDown(float x,
                         float y,
                         blink::WebPointerProperties::Button button =
                             blink::WebPointerProperties::Button::kLeft) {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown, {x, y}, {x, y}, button,
        /*click_count_param=*/1, blink::WebInputEvent::kNoModifiers,
        base::TimeTicks::Now());
    web_contents_->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->ForwardMouseEvent(mouse_event);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::map<ui::AXTreeID, std::unique_ptr<ui::AXTreeManager>> ax_tree_managers_;
  SkillRecordingService service_{&profile_, /*model_executor=*/nullptr};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TabActiveObserver> observer_;
};

TEST_F(TabActiveObserverTest, RecordsLeftMouseDownOnHttpAndHttpsUrls) {
  NavigateTo(GURL("https://example.com/secure"));
  task_environment_.FastForwardBy(base::Milliseconds(100));
  SimulateMouseDown(15.0f, 25.0f);
  // Clicking non-interactive blank space outside the `(0, 0, 300, 300)` button
  // should be ignored.
  SimulateMouseDown(500.0f, 500.0f);

  NavigateTo(GURL("http://example.com/insecure"));
  task_environment_.FastForwardBy(base::Milliseconds(200));
  SimulateMouseDown(35.0f, 45.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[0].description, "Clicked on 'Submit'");
  EXPECT_EQ(result.actions[0].timestamp, base::Milliseconds(100));
  EXPECT_EQ(result.actions[1].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[1].description, "Clicked on 'Submit'");
  EXPECT_EQ(result.actions[1].timestamp, base::Milliseconds(300));
}

TEST_F(TabActiveObserverTest, IgnoresNonMouseDownEvents) {
  NavigateTo(GURL("https://example.com"));

  blink::WebGestureEvent scroll_event(
      blink::WebInputEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
      blink::WebGestureDevice::kTouchscreen);
  web_contents_->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->SimulateUserInteraction(scroll_event);

  blink::WebMouseEvent mouse_up_event(
      blink::WebInputEvent::Type::kMouseUp, {10.0f, 20.0f}, {10.0f, 20.0f},
      blink::WebPointerProperties::Button::kLeft,
      /*click_count_param=*/1, blink::WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  web_contents_->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->ForwardMouseEvent(mouse_up_event);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest, IgnoresNonLeftMouseButtons) {
  NavigateTo(GURL("https://example.com"));

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  blink::WebMouseEvent right_down(
      blink::WebInputEvent::Type::kMouseDown, {10.0f, 20.0f}, {10.0f, 20.0f},
      blink::WebPointerProperties::Button::kRight, /*click_count_param=*/1,
      blink::WebInputEvent::kNoModifiers, timestamp);
  web_contents_->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->ForwardMouseEvent(right_down);
  // Verify `OnInputEvent()` did not populate `pending_mouse_down_` for a
  // non-left click.
  blink::WebMouseEvent synthetic_left_down(
      blink::WebInputEvent::Type::kMouseDown, {10.0f, 20.0f}, {10.0f, 20.0f},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::kNoModifiers, timestamp);
  web_contents_->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->SimulateUserInteraction(synthetic_left_down);

  SimulateMouseDown(30.0f, 40.0f, blink::WebPointerProperties::Button::kMiddle);
  SimulateMouseDown(50.0f, 60.0f, blink::WebPointerProperties::Button::kBack);
  SimulateMouseDown(70.0f, 80.0f,
                    blink::WebPointerProperties::Button::kForward);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest, IgnoresNonHttpOrHttpsSchemesAndErrorDocuments) {
  NavigateTo(GURL(chrome::kChromeUIContextHubURL));
  SimulateMouseDown(10.0f, 20.0f);

  NavigateTo(GURL("about:blank"));
  SimulateMouseDown(30.0f, 40.0f);

  NavigateTo(GURL("file:///tmp/test.html"));
  SimulateMouseDown(50.0f, 60.0f);

  // Committed error pages retain the failed HTTPS target URL in
  // `GetLastCommittedURL()`, so clicks on error page controls must be ignored.
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents_.get(), GURL("https://example.com/broken"),
      net::ERR_TIMED_OUT);
  ASSERT_TRUE(web_contents_->GetPrimaryMainFrame()->IsErrorDocument());
  ASSERT_EQ(web_contents_->GetLastCommittedURL(),
            GURL("https://example.com/broken"));
  PrepareMainFrameForInput();
  SetDefaultClickableAXTreeForFrame(web_contents_->GetPrimaryMainFrame());
  SimulateMouseDown(15.0f, 25.0f);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest,
       RecordsClicksAcrossCrossOriginNavigationAndDynamicSameOriginSubframe) {
  NavigateTo(GURL("https://example.com/first"));
  content::RenderWidgetHost* first_rwh =
      web_contents_->GetPrimaryMainFrame()->GetRenderWidgetHost();

  // Navigate cross-origin without recreating `observer_` so the new main frame
  // widget is observed via `RenderFrameCreated()` / `RenderFrameHostChanged()`.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents_.get(), GURL("https://other.example/second"));
  content::RenderFrameHost* second_main_rfh =
      web_contents_->GetPrimaryMainFrame();
  ASSERT_NE(second_main_rfh->GetRenderWidgetHost(), first_rwh);
  PrepareMainFrameForInput();
  SetDefaultClickableAXTreeForFrame(second_main_rfh, "Second Page Button");
  SimulateMouseDown(15.0f, 25.0f);

  // Dynamically attach and commit a same-origin subframe while `observer_` is
  // alive and verify clicks routed into the subframe are resolved.
  content::RenderFrameHost* sub_rfh =
      content::RenderFrameHostTester::For(second_main_rfh)
          ->AppendChild("same_origin_sub");
  sub_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.example/sub"), sub_rfh);
  content::RenderFrameHostTester::For(sub_rfh)->InitializeRenderFrameIfNeeded();
  web_contents_->WasHidden();
  web_contents_->WasShown();
  SetDefaultClickableAXTreeForFrame(sub_rfh, "Subframe Button");

  ui::AXNodeData main_root;
  main_root.id = 1;
  main_root.role = ax::mojom::Role::kRootWebArea;
  main_root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  main_root.child_ids = {2};

  ui::AXNodeData iframe_node;
  iframe_node.id = 2;
  iframe_node.role = ax::mojom::Role::kIframe;
  iframe_node.relative_bounds.bounds = gfx::RectF(100, 100, 200, 150);
  iframe_node.AddChildTreeId(sub_rfh->GetAXTreeID());

  const ui::AXTreeID main_tree_id = second_main_rfh->GetAXTreeID();
  ui::AXTreeUpdate main_update;
  main_update.has_tree_data = true;
  main_update.tree_data.tree_id = main_tree_id;
  main_update.root_id = 1;
  main_update.nodes = {main_root, iframe_node};
  ax_tree_managers_.erase(main_tree_id);
  ax_tree_managers_.emplace(main_tree_id,
                            std::make_unique<ui::AXTreeManager>(
                                std::make_unique<ui::AXTree>(main_update)));

  SimulateMouseDown(120.0f, 120.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[0].description, "Clicked on 'Second Page Button'");
  EXPECT_EQ(result.actions[1].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[1].description, "Clicked on 'Subframe Button'");
}

TEST_F(TabActiveObserverTest, IgnoresCrossOriginSubframeAndUnroutedMouseDowns) {
  NavigateTo(GURL("https://example.com/main"));

  content::RenderFrameHost* main_rfh = web_contents_->GetPrimaryMainFrame();
  content::RenderFrameHost* cross_origin_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("cross");
  cross_origin_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://other.example/widget"), cross_origin_rfh);
  content::RenderFrameHostTester::For(cross_origin_rfh)
      ->InitializeRenderFrameIfNeeded();
  web_contents_->WasHidden();
  web_contents_->WasShown();
  SetDefaultClickableAXTreeForFrame(cross_origin_rfh);

  blink::WebMouseEvent cross_click(
      blink::WebInputEvent::Type::kMouseDown, {10.0f, 10.0f}, {10.0f, 10.0f},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  cross_origin_rfh->GetRenderWidgetHost()->ForwardMouseEvent(cross_click);

  // An unrouted `DidGetUserInteraction()` without `OnInputEvent()` must fail
  // closed rather than falling back to `main_rfh`.
  main_rfh->GetRenderWidgetHost()->SimulateUserInteraction(cross_click);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest, IgnoresInnerWebContents) {
  NavigateTo(GURL("https://example.com/main"));

  content::RenderFrameHost* main_rfh = web_contents_->GetPrimaryMainFrame();
  content::RenderFrameHost* attach_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("guest_host");
  std::unique_ptr<content::WebContents> inner_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  content::WebContents* raw_inner = inner_contents.get();
  content::WebContentsTester::For(raw_inner)->NavigateAndCommit(
      GURL("https://example.com/inner"));
  content::RenderFrameHostTester::For(raw_inner->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  raw_inner->WasHidden();
  raw_inner->WasShown();
  SetDefaultClickableAXTreeForFrame(raw_inner->GetPrimaryMainFrame());
  web_contents_->AttachInnerWebContents(std::move(inner_contents), attach_rfh,
                                        /*is_full_page=*/false);

  // Recreate `observer_` while `inner_contents` is attached so constructor
  // `ForEachRenderFrameHost()` traversal is exercised as well.
  observer_.reset();
  observer_ =
      std::make_unique<TabActiveObserver>(&service_, web_contents_.get());

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  blink::WebMouseEvent inner_click(
      blink::WebInputEvent::Type::kMouseDown, {10.0f, 10.0f}, {10.0f, 10.0f},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::kNoModifiers, timestamp);
  raw_inner->GetPrimaryMainFrame()->GetRenderWidgetHost()->ForwardMouseEvent(
      inner_click);
  main_rfh->GetRenderWidgetHost()->SimulateUserInteraction(inner_click);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest, FallsBackWhenAxTreeIsUnavailable) {
  // Fresh HTTP/HTTPS navigation whose `AXTree` has not been populated yet
  // falls back to recording widget coordinates instead of dropping the click.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/fresh"));
  PrepareMainFrameForInput();
  service_.StopRecording();
  service_.StartRecording();
  SimulateMouseDown(15.6f, 24.4f);

  // An in-process same-origin child iframe whose `AXTree` is not populated yet
  // also falls back to main-frame coordinates.
  content::RenderFrameHost* main_rfh = web_contents_->GetPrimaryMainFrame();
  content::RenderFrameHost* same_sub_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("same_sub");
  same_sub_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/sub"), same_sub_rfh);
  ui::AXNodeData main_root;
  main_root.id = 1;
  main_root.role = ax::mojom::Role::kRootWebArea;
  main_root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  main_root.child_ids = {2};

  ui::AXNodeData iframe_node;
  iframe_node.id = 2;
  iframe_node.role = ax::mojom::Role::kIframe;
  iframe_node.relative_bounds.bounds = gfx::RectF(100, 100, 200, 150);
  iframe_node.AddChildTreeId(same_sub_rfh->GetAXTreeID());

  const ui::AXTreeID main_tree_id = main_rfh->GetAXTreeID();
  ui::AXTreeUpdate main_update;
  main_update.has_tree_data = true;
  main_update.tree_data.tree_id = main_tree_id;
  main_update.root_id = 1;
  main_update.nodes = {main_root, iframe_node};
  ax_tree_managers_.emplace(main_tree_id,
                            std::make_unique<ui::AXTreeManager>(
                                std::make_unique<ui::AXTree>(main_update)));
  SimulateMouseDown(120.4f, 130.6f);

  // Blank space outside the iframe (with main-frame `AXTree` available) is
  // still ignored.
  SimulateMouseDown(500.0f, 500.0f);

  // Cross-origin subframes without an `AXTree` must still be ignored.
  content::RenderFrameHost* cross_origin_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("cross");
  cross_origin_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://other.example/widget"), cross_origin_rfh);
  content::RenderFrameHostTester::For(cross_origin_rfh)
      ->InitializeRenderFrameIfNeeded();
  web_contents_->WasHidden();
  web_contents_->WasShown();
  blink::WebMouseEvent cross_click(
      blink::WebInputEvent::Type::kMouseDown, {10.0f, 10.0f}, {10.0f, 10.0f},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  cross_origin_rfh->GetRenderWidgetHost()->ForwardMouseEvent(cross_click);

  // Non-HTTP/HTTPS pages without an `AXTree` must still be ignored.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL(chrome::kChromeUIContextHubURL));
  PrepareMainFrameForInput();
  SimulateMouseDown(20.0f, 30.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[0].description, "Clicked at X:16, Y:24");
  EXPECT_EQ(result.actions[1].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[1].description, "Clicked at X:120, Y:131");
}

}  // namespace
}  // namespace context_hub
