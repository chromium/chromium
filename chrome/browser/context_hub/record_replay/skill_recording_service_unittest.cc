// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

class TestBrowserInstance {
 public:
  explicit TestBrowserInstance(Profile* profile)
      : profile_(CHECK_DEREF(profile)),
        tab_strip_model_(&tab_strip_model_delegate_, profile) {
    ON_CALL(mock_bwi_, GetProfile())
        .WillByDefault(testing::Return(&profile_.get()));
    ON_CALL(mock_bwi_, GetTabStripModel())
        .WillByDefault(testing::Return(&tab_strip_model_));
    ON_CALL(mock_bwi_, RegisterDidBecomeActive(testing::_))
        .WillByDefault(
            [this](BrowserWindowInterface::DidBecomeActiveCallback cb) {
              return active_callbacks_.Add(std::move(cb));
            });
    ON_CALL(mock_bwi_, RegisterBrowserDidClose(testing::_))
        .WillByDefault(
            [this](BrowserWindowInterface::BrowserDidCloseCallback cb) {
              return close_callbacks_.Add(std::move(cb));
            });

    BrowserManagerServiceFactory::GetForProfile(&profile_.get())
        ->AddBrowserForTesting(&mock_bwi_);
  }

  TestBrowserInstance(const TestBrowserInstance&) = delete;
  TestBrowserInstance& operator=(const TestBrowserInstance&) = delete;

  ~TestBrowserInstance() {
    tab_strip_model_.CloseAllTabs();
    close_callbacks_.Notify(&mock_bwi_);
  }

  TabStripModel* tab_strip_model() { return &tab_strip_model_; }
  BrowserWindowInterface* bwi() { return &mock_bwi_; }

  void Activate() { active_callbacks_.Notify(&mock_bwi_); }

 private:
  const raw_ref<Profile> profile_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  TabStripModel tab_strip_model_;
  testing::NiceMock<MockBrowserWindowInterface> mock_bwi_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)> active_callbacks_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)> close_callbacks_;
};

class SkillRecordingServiceTest : public testing::Test {
 public:
  SkillRecordingServiceTest() = default;
  ~SkillRecordingServiceTest() override = default;

 protected:
  content::WebContents* AddTab(TestBrowserInstance* browser_instance,
                               const GURL& url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(
            browser_instance->bwi()->GetProfile(),
            content::SiteInstance::Create(
                browser_instance->bwi()->GetProfile()));
    content::WebContents* raw_contents = contents.get();
    content::WebContentsTester::For(raw_contents)->NavigateAndCommit(url);
    browser_instance->tab_strip_model()->InsertWebContentsAt(
        browser_instance->tab_strip_model()->count(), std::move(contents),
        AddTabTypes::ADD_INHERIT_OPENER | AddTabTypes::ADD_ACTIVE);
    return raw_contents;
  }

  void SimulateMouseDown(content::WebContents* web_contents,
                         float x,
                         float y,
                         blink::WebPointerProperties::Button button =
                             blink::WebPointerProperties::Button::kLeft) {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown, {x, y}, {x, y}, button,
        /*click_count_param=*/1, blink::WebInputEvent::kNoModifiers,
        base::TimeTicks::Now());
    web_contents->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->SimulateUserInteraction(mouse_event);
  }

  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_tab_features_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  SkillRecordingService service_{&profile_};
};

TEST_F(SkillRecordingServiceTest, InitialState) {
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
  EXPECT_FALSE(service_.is_recording());
}

TEST_F(SkillRecordingServiceTest, StartAndStopRecording) {
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  EXPECT_EQ(service_.start_time(), base::TimeTicks::Now());

  auto result = service_.StopRecording();
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
}

TEST_F(SkillRecordingServiceTest, RecordClickWhileRecording) {
  service_.StartRecording();
  task_environment_.FastForwardBy(base::Milliseconds(100));
  service_.RecordClick(10, 20);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  service_.RecordClick(30, 40);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 2u);
  EXPECT_EQ(result->actions[0]->description, "Mouse Clicked at X:10, Y:20");
  EXPECT_EQ(result->actions[0]->timestamp, base::Milliseconds(100));
  EXPECT_EQ(result->actions[1]->description, "Mouse Clicked at X:30, Y:40");
  EXPECT_EQ(result->actions[1]->timestamp, base::Milliseconds(350));

  // Calling `StopRecording()` again should return empty.
  auto second_result = service_.StopRecording();
  ASSERT_TRUE(second_result);
  EXPECT_TRUE(second_result->actions.empty());
}

TEST_F(SkillRecordingServiceTest, IgnoresRecordClickWhenNotRecording) {
  service_.RecordClick(10, 20);

  service_.StartRecording();
  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
}

TEST_F(SkillRecordingServiceTest, IgnoresMultipleStartRecording) {
  const base::TimeTicks initial_start_time = base::TimeTicks::Now();
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  EXPECT_EQ(service_.start_time(), initial_start_time);
  service_.RecordClick(10, 20);

  task_environment_.FastForwardBy(base::Seconds(5));

  // Calling `StartRecording()` again while already recording should be a no-op
  // and not reset actions.
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  // The start time should be unchanged.
  EXPECT_EQ(service_.start_time(), initial_start_time);
  service_.RecordClick(30, 40);

  auto result = service_.StopRecording();
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 2u);
}

TEST_F(SkillRecordingServiceTest, ShouldTrackBrowser_MatchesSameProfileOnly) {
  TestBrowserInstance same_profile_browser(&profile_);
  EXPECT_TRUE(service_.ShouldTrackBrowser(same_profile_browser.bwi()));
  EXPECT_FALSE(service_.ShouldTrackBrowser(nullptr));

  Profile* otr_profile = profile_.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  TestBrowserInstance otr_browser(otr_profile);
  EXPECT_FALSE(service_.ShouldTrackBrowser(otr_browser.bwi()));

  TestingProfile other_profile;
  TestBrowserInstance other_browser(&other_profile);
  EXPECT_FALSE(service_.ShouldTrackBrowser(other_browser.bwi()));
}

TEST_F(SkillRecordingServiceTest, RecordsLeftMouseClicksOnHttpOrHttpsTab) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* tab =
      AddTab(&browser, GURL("https://example.com/page"));

  service_.StartRecording();
  task_environment_.FastForwardBy(base::Milliseconds(150));
  SimulateMouseDown(tab, 42.0f, 84.0f,
                    blink::WebPointerProperties::Button::kLeft);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 1u);
  EXPECT_EQ(result->actions[0]->description, "Mouse Clicked at X:42, Y:84");
  EXPECT_EQ(result->actions[0]->timestamp, base::Milliseconds(150));
}

TEST_F(SkillRecordingServiceTest, IgnoresNonLeftMouseButtonClicks) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* tab =
      AddTab(&browser, GURL("https://example.com/page"));

  service_.StartRecording();
  SimulateMouseDown(tab, 10.0f, 20.0f,
                    blink::WebPointerProperties::Button::kRight);
  SimulateMouseDown(tab, 30.0f, 40.0f,
                    blink::WebPointerProperties::Button::kMiddle);
  SimulateMouseDown(tab, 50.0f, 60.0f,
                    blink::WebPointerProperties::Button::kBack);
  SimulateMouseDown(tab, 70.0f, 80.0f,
                    blink::WebPointerProperties::Button::kForward);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
}

TEST_F(SkillRecordingServiceTest, IgnoresClicksOnContextHubAndNonHttpUrls) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* web_tab =
      AddTab(&browser, GURL("https://example.com/page"));
  content::WebContents* context_hub_tab =
      AddTab(&browser, GURL(chrome::kChromeUIContextHubURL));

  service_.StartRecording();

  // Clicking on `chrome://context-hub` (e.g., "Stop Recording") must be
  // ignored.
  SimulateMouseDown(context_hub_tab, 100.0f, 200.0f);

  // Switching to the HTTPS tab and clicking should record.
  browser.tab_strip_model()->ActivateTabAt(0);
  SimulateMouseDown(web_tab, 55.0f, 65.0f);

  // Switching back to `chrome://context-hub` to stop recording and clicking
  // must not add a phantom click.
  browser.tab_strip_model()->ActivateTabAt(1);
  SimulateMouseDown(context_hub_tab, 100.0f, 200.0f);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 1u);
  EXPECT_EQ(result->actions[0]->description, "Mouse Clicked at X:55, Y:65");
}

TEST_F(SkillRecordingServiceTest,
       SwitchesObservedTabWhenBrowserWindowActivationChanges) {
  TestBrowserInstance window1(&profile_);
  content::WebContents* tab1 =
      AddTab(&window1, GURL("https://example.com/window1"));

  TestBrowserInstance window2(&profile_);
  content::WebContents* tab2 =
      AddTab(&window2, GURL("https://example.com/window2"));

  // Make window2 the most recently active window before starting recording.
  window2.Activate();

  service_.StartRecording();

  // Initial observation should attach to `window2` (the active window), not
  // `window1` (the older inactive window).
  SimulateMouseDown(tab1, 11.0f, 22.0f);
  SimulateMouseDown(tab2, 33.0f, 44.0f);

  // Now switch window focus to `window1` without changing tabs inside
  // `window1`'s tab strip.
  window1.Activate();
  SimulateMouseDown(tab2, 55.0f, 66.0f);
  SimulateMouseDown(tab1, 77.0f, 88.0f);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 2u);
  EXPECT_EQ(result->actions[0]->description, "Mouse Clicked at X:33, Y:44");
  EXPECT_EQ(result->actions[1]->description, "Mouse Clicked at X:77, Y:88");
}

TEST_F(SkillRecordingServiceTest, IgnoresClicksFromOtherProfiles) {
  TestBrowserInstance main_window(&profile_);
  content::WebContents* main_tab =
      AddTab(&main_window, GURL("https://example.com/main"));
  main_window.Activate();

  TestingProfile other_profile;
  TestBrowserInstance other_window(&other_profile);
  content::WebContents* other_tab =
      AddTab(&other_window, GURL("https://example.com/other"));
  other_window.Activate();

  service_.StartRecording();

  SimulateMouseDown(other_tab, 10.0f, 20.0f);
  SimulateMouseDown(main_tab, 30.0f, 40.0f);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 1u);
  EXPECT_EQ(result->actions[0]->description, "Mouse Clicked at X:30, Y:40");
}

TEST_F(SkillRecordingServiceTest,
       UpdatesObservedTabWhenActiveWebContentsIsReplaced) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* original_tab =
      AddTab(&browser, GURL("https://example.com/original"));

  service_.StartRecording();

  std::unique_ptr<content::WebContents> replacement_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  content::WebContents* raw_replacement = replacement_contents.get();
  content::WebContentsTester::For(raw_replacement)
      ->NavigateAndCommit(GURL("https://example.com/replacement"));

  std::unique_ptr<content::WebContents> old_contents =
      browser.tab_strip_model()->DiscardWebContents(
          original_tab, std::move(replacement_contents));

  SimulateMouseDown(old_contents.get(), 10.0f, 20.0f);
  SimulateMouseDown(raw_replacement, 50.0f, 60.0f);

  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->actions.size(), 1u);
  EXPECT_EQ(result->actions[0]->description, "Mouse Clicked at X:50, Y:60");
}

}  // namespace
}  // namespace context_hub
