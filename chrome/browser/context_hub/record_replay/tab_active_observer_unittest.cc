// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

using browser::context_hub::mojom::SkillRecordingActionType;

class TabActiveObserverTest : public testing::Test {
 public:
  TabActiveObserverTest() = default;
  ~TabActiveObserverTest() override = default;

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &profile_, content::SiteInstance::Create(&profile_));
    service_.StartRecording();
    observer_ =
        std::make_unique<TabActiveObserver>(&service_, web_contents_.get());
  }

  void TearDown() override {
    observer_.reset();
    web_contents_.reset();
  }

 protected:
  void NavigateTo(const GURL& url) {
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(url);
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
        ->SimulateUserInteraction(mouse_event);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  SkillRecordingService service_{&profile_};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TabActiveObserver> observer_;
};

TEST_F(TabActiveObserverTest, RecordsLeftMouseDownOnHttpAndHttpsUrls) {
  NavigateTo(GURL("https://example.com/secure"));
  task_environment_.FastForwardBy(base::Milliseconds(100));
  SimulateMouseDown(15.0f, 25.0f);

  NavigateTo(GURL("http://example.com/insecure"));
  task_environment_.FastForwardBy(base::Milliseconds(200));
  SimulateMouseDown(35.0f, 45.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:15, Y:25");
  EXPECT_EQ(result.actions[0].timestamp, base::Milliseconds(100));
  EXPECT_EQ(result.actions[1].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[1].description, "Mouse Clicked at X:35, Y:45");
  EXPECT_EQ(result.actions[1].timestamp, base::Milliseconds(300));
}

TEST_F(TabActiveObserverTest, RoundsFractionalCoordinates) {
  NavigateTo(GURL("https://example.com"));
  SimulateMouseDown(10.4f, 20.6f);
  SimulateMouseDown(10.5f, 20.49f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:10, Y:21");
  EXPECT_EQ(result.actions[1].description, "Mouse Clicked at X:11, Y:20");
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
  observer_->DidGetUserInteraction(mouse_up_event);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest, IgnoresNonLeftMouseButtons) {
  NavigateTo(GURL("https://example.com"));

  SimulateMouseDown(10.0f, 20.0f, blink::WebPointerProperties::Button::kRight);
  SimulateMouseDown(30.0f, 40.0f, blink::WebPointerProperties::Button::kMiddle);
  SimulateMouseDown(50.0f, 60.0f, blink::WebPointerProperties::Button::kBack);
  SimulateMouseDown(70.0f, 80.0f,
                    blink::WebPointerProperties::Button::kForward);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(TabActiveObserverTest, IgnoresNonHttpOrHttpsSchemes) {
  NavigateTo(GURL(chrome::kChromeUIContextHubURL));
  SimulateMouseDown(10.0f, 20.0f);

  NavigateTo(GURL("about:blank"));
  SimulateMouseDown(30.0f, 40.0f);

  NavigateTo(GURL("file:///tmp/test.html"));
  SimulateMouseDown(50.0f, 60.0f);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

}  // namespace
}  // namespace context_hub
