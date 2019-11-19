// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_manager.h"

#include <memory>
#include <vector>

#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_ukm_logger.h"
#include "chrome/browser/chromeos/power/ml/screen_brightness_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {
struct LogActivityInfo {
  ScreenBrightnessEvent screen_brightness_event;
  ukm::SourceId tab_id;
  bool has_form_entry;
};

const int kInactivityDurationSecs =
    AdaptiveScreenBrightnessManager::kInactivityDuration.InSeconds();
const int kLoggingIntervalSecs =
    AdaptiveScreenBrightnessManager::kLoggingInterval.InSeconds();

// Testing ukm logger.
class TestingAdaptiveScreenBrightnessUkmLogger
    : public AdaptiveScreenBrightnessUkmLogger {
 public:
  TestingAdaptiveScreenBrightnessUkmLogger() = default;
  ~TestingAdaptiveScreenBrightnessUkmLogger() override = default;

  const std::vector<LogActivityInfo>& log_activity_info() const {
    return log_activity_info_;
  }

  // AdaptiveScreenBrightnessUkmLogger overrides:
  void LogActivity(const ScreenBrightnessEvent& screen_brightness_event,
                   ukm::SourceId tab_id,
                   bool has_form_entry) override {
    log_activity_info_.push_back(
        LogActivityInfo{screen_brightness_event, tab_id, has_form_entry});
  }

 private:
  std::vector<LogActivityInfo> log_activity_info_;

  DISALLOW_COPY_AND_ASSIGN(TestingAdaptiveScreenBrightnessUkmLogger);
};

}  // namespace

class AdaptiveScreenBrightnessManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AdaptiveScreenBrightnessManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  ~AdaptiveScreenBrightnessManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PowerManagerClient::InitializeFake();
    auto logger = std::make_unique<TestingAdaptiveScreenBrightnessUkmLogger>();
    ukm_logger_ = logger.get();

    mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer;
    auto periodic_timer = std::make_unique<base::RepeatingTimer>();
    periodic_timer->SetTaskRunner(
        task_environment()->GetMainThreadTaskRunner());
    screen_brightness_manager_ =
        std::make_unique<AdaptiveScreenBrightnessManager>(
            std::move(logger), &user_activity_detector_,
            FakePowerManagerClient::Get(), nullptr, nullptr,
            observer.InitWithNewPipeAndPassReceiver(),
            std::move(periodic_timer));
  }

  void TearDown() override {
    screen_brightness_manager_.reset();
    PowerManagerClient::Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestingAdaptiveScreenBrightnessUkmLogger* ukm_logger() { return ukm_logger_; }

  void ReportUserActivity(const ui::Event* const event) {
    screen_brightness_manager_->OnUserActivity(event);
  }

  void ReportPowerChangeEvent(
      const power_manager::PowerSupplyProperties::ExternalPower power,
      const float battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_external_power(power);
    proto.set_battery_percent(battery_percent);
    FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
  }

  void ReportLidEvent(const PowerManagerClient::LidState state) {
    FakePowerManagerClient::Get()->SetLidState(state,
                                               base::TimeTicks::UnixEpoch());
  }

  void ReportTabletModeEvent(const PowerManagerClient::TabletMode mode) {
    FakePowerManagerClient::Get()->SetTabletMode(mode,
                                                 base::TimeTicks::UnixEpoch());
  }

  void ReportBrightnessChangeEvent(
      const double level,
      const power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(level);
    change.set_cause(cause);
    screen_brightness_manager_->ScreenBrightnessChanged(change);
  }

  void ReportVideoStart() {
    screen_brightness_manager_->OnVideoActivityStarted();
  }

  void ReportVideoEnd() { screen_brightness_manager_->OnVideoActivityEnded(); }

  void FireTimer() { screen_brightness_manager_->OnTimerFired(); }

  void InitializeBrightness(const double level) {
    screen_brightness_manager_->OnReceiveScreenBrightnessPercent(level);
  }

  void FastForwardTimeBySecs(const int seconds) {
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(seconds));
  }

  // Creates a test browser window and sets its visibility, activity and
  // incognito status.
  std::unique_ptr<Browser> CreateTestBrowser(bool is_visible,
                                             bool is_focused,
                                             bool is_incognito = false) {
    Profile* const original_profile = profile();
    Profile* const used_profile =
        is_incognito ? original_profile->GetOffTheRecordProfile()
                     : original_profile;
    Browser::CreateParams params(used_profile, true);

    auto dummy_window = std::make_unique<aura::Window>(nullptr);
    dummy_window->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(dummy_window.get());
    dummy_window->SetBounds(gfx::Rect(root_window()->bounds().size()));
    if (is_visible) {
      dummy_window->Show();
    } else {
      dummy_window->Hide();
    }

    std::unique_ptr<Browser> browser =
        chrome::CreateBrowserWithAuraTestWindowForParams(
            std::move(dummy_window), &params);
    if (is_focused) {
      browser->window()->Activate();
    } else {
      browser->window()->Deactivate();
    }
    return browser;
  }

  // Adds a tab with specified url to the tab strip model. Also optionally sets
  // the tab to be the active one in the tab strip model.
  // TODO(jiameng): there doesn't seem to be a way to set form entry (via
  // page importance signal). Check if there's some other way to set it.
  ukm::SourceId CreateTestWebContents(TabStripModel* const tab_strip_model,
                                      const GURL& url,
                                      bool is_active) {
    DCHECK(tab_strip_model);
    DCHECK(!url.is_empty());
    content::WebContents* contents =
        tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model, url);
    if (is_active) {
      tab_strip_model->ActivateTabAt(tab_strip_model->count() - 1);
    }
    content::WebContentsTester::For(contents)->TestSetIsLoading(false);
    return ukm::GetSourceIdForWebContentsDocument(contents);
  }

  const gfx::Point kEventLocation = gfx::Point(90, 90);
  const ui::MouseEvent kMouseEvent = ui::MouseEvent(ui::ET_MOUSE_MOVED,
                                                    kEventLocation,
                                                    kEventLocation,
                                                    base::TimeTicks(),
                                                    0,
                                                    0);

  TabActivitySimulator tab_activity_simulator_;
  const GURL kUrl1 = GURL("https://example1.com/");
  const GURL kUrl2 = GURL("https://example2.com/");
  const GURL kUrl3 = GURL("https://example3.com/");

 private:
  FakeChromeUserManager fake_user_manager_;

  ui::UserActivityDetector user_activity_detector_;
  std::unique_ptr<AdaptiveScreenBrightnessManager> screen_brightness_manager_;
  TestingAdaptiveScreenBrightnessUkmLogger* ukm_logger_;

  DISALLOW_COPY_AND_ASSIGN(AdaptiveScreenBrightnessManagerTest);
};

TEST_F(AdaptiveScreenBrightnessManagerTest, PeriodicLogging) {
  InitializeBrightness(75.0f);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  ReportVideoStart();
  ReportLidEvent(PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(PowerManagerClient::TabletMode::UNSUPPORTED);

  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_FLOAT_EQ(23.0, features.env_data().battery_percent());
  EXPECT_FALSE(features.env_data().on_battery());
  EXPECT_TRUE(features.activity_data().is_video_playing());
  EXPECT_EQ(ScreenBrightnessEvent::Features::EnvData::LAPTOP,
            features.env_data().device_mode());
  EXPECT_EQ(75, features.env_data().previous_brightness());

  const ScreenBrightnessEvent::Event& event =
      info[0].screen_brightness_event.event();
  EXPECT_EQ(75, event.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::PERIODIC, event.reason());
  EXPECT_FALSE(event.has_time_since_last_event_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest,
       PeriodicLoggingBrightnessUninitialized) {
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  ReportVideoStart();
  ReportLidEvent(PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(PowerManagerClient::TabletMode::UNSUPPORTED);

  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  EXPECT_TRUE(info.empty());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, PeriodicTimerTest) {
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  ReportVideoStart();
  ReportLidEvent(PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(PowerManagerClient::TabletMode::UNSUPPORTED);

  FastForwardTimeBySecs(kLoggingIntervalSecs - 10);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  EXPECT_TRUE(info.empty());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, BrightnessChange) {
  ReportBrightnessChangeEvent(
      30.0f, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  FastForwardTimeBySecs(2);
  ReportBrightnessChangeEvent(
      40.0f, power_manager::
                 BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED);
  FastForwardTimeBySecs(6);
  ReportBrightnessChangeEvent(
      20.0f, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(3U, info.size());

  const ScreenBrightnessEvent::Event& event =
      info[0].screen_brightness_event.event();
  EXPECT_EQ(30, event.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::OTHER, event.reason());
  EXPECT_FALSE(event.has_time_since_last_event_sec());
  // Brightness wasn't initialized so there's no previous brightness level.
  EXPECT_FALSE(info[0]
                   .screen_brightness_event.features()
                   .env_data()
                   .has_previous_brightness());

  const ScreenBrightnessEvent::Event& event1 =
      info[1].screen_brightness_event.event();
  EXPECT_EQ(40, event1.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::EXTERNAL_POWER_DISCONNECTED,
            event1.reason());
  EXPECT_EQ(2, event1.time_since_last_event_sec());
  EXPECT_EQ(30, info[1]
                    .screen_brightness_event.features()
                    .env_data()
                    .previous_brightness());

  const ScreenBrightnessEvent::Event& event2 =
      info[2].screen_brightness_event.event();
  EXPECT_EQ(20, event2.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::USER_DOWN, event2.reason());
  EXPECT_EQ(6, event2.time_since_last_event_sec());
  EXPECT_EQ(40, info[2]
                    .screen_brightness_event.features()
                    .env_data()
                    .previous_brightness());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, NoUserEvents) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_FALSE(features.activity_data().has_last_activity_time_sec());
  EXPECT_FALSE(features.activity_data().has_recent_time_active_sec());
  EXPECT_EQ(75, features.env_data().previous_brightness());

  const ScreenBrightnessEvent::Event& event =
      info[0].screen_brightness_event.event();
  EXPECT_EQ(75, event.brightness());
  EXPECT_FALSE(event.has_time_since_last_event_sec());
  EXPECT_EQ(ScreenBrightnessEvent::Event::PERIODIC, event.reason());

  EXPECT_EQ(ukm::kInvalidSourceId, info[0].tab_id);
}

TEST_F(AdaptiveScreenBrightnessManagerTest, NullUserActivity) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(nullptr);
  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_FALSE(features.activity_data().has_last_activity_time_sec());
  EXPECT_FALSE(features.activity_data().has_recent_time_active_sec());
  EXPECT_EQ(75, features.env_data().previous_brightness());

  const ScreenBrightnessEvent::Event& event =
      info[0].screen_brightness_event.event();
  EXPECT_EQ(75, event.brightness());
  EXPECT_FALSE(event.has_time_since_last_event_sec());
  EXPECT_EQ(ScreenBrightnessEvent::Event::PERIODIC, event.reason());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, OneUserEvent) {
  InitializeBrightness(75.0f);

  ReportUserActivity(&kMouseEvent);
  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_EQ(kLoggingIntervalSecs,
            features.activity_data().last_activity_time_sec());
  EXPECT_EQ(0, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, TwoUserEventsSameActivity) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  // Timer starts from the beginning, so subtract 6 seconds.
  EXPECT_EQ(kLoggingIntervalSecs - 6,
            features.activity_data().last_activity_time_sec());
  EXPECT_EQ(5, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, TwoUserEventsDifferentActivities) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kInactivityDurationSecs + 5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kLoggingIntervalSecs);

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_EQ(kLoggingIntervalSecs - kInactivityDurationSecs - 6,
            features.activity_data().last_activity_time_sec());
  EXPECT_EQ(0, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest,
       MultipleUserEventsMultipleActivities) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kInactivityDurationSecs + 5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kInactivityDurationSecs + 10);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_EQ(2, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(4, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, VideoStartStop) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(2);
  ReportVideoStart();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 40);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(2U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_EQ(0, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(5, features.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features1 =
      info[1].screen_brightness_event.features();
  EXPECT_EQ(0, features1.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 45,
            features1.activity_data().recent_time_active_sec());

  FastForwardTimeBySecs(10);
  ReportVideoEnd();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 45);
  FireTimer();

  ASSERT_EQ(4U, info.size());
  const ScreenBrightnessEvent::Features& features2 =
      info[2].screen_brightness_event.features();
  EXPECT_EQ(5, features2.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 55,
            features2.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features3 =
      info[3].screen_brightness_event.features();
  EXPECT_EQ(kInactivityDurationSecs + 50,
            features3.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 55,
            features3.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, VideoStartStopWithUserEvents) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  ReportVideoStart();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 40);
  FireTimer();

  FastForwardTimeBySecs(4);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(6);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(3U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_EQ(0, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(7, features.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features1 =
      info[1].screen_brightness_event.features();
  EXPECT_EQ(0, features1.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 47,
            features1.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features2 =
      info[2].screen_brightness_event.features();
  EXPECT_EQ(0, features2.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 57,
            features2.activity_data().recent_time_active_sec());

  FastForwardTimeBySecs(10);
  ReportVideoEnd();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 45);
  FireTimer();

  ASSERT_EQ(5U, info.size());
  const ScreenBrightnessEvent::Features& features3 =
      info[3].screen_brightness_event.features();
  EXPECT_EQ(5, features3.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 67,
            features3.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features4 =
      info[4].screen_brightness_event.features();
  EXPECT_EQ(kInactivityDurationSecs + 50,
            features4.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 67,
            features4.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, UserEventCounts) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  const ui::TouchEvent kTouchEvent(
      ui::ET_TOUCH_PRESSED, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ReportUserActivity(&kTouchEvent);
  ReportUserActivity(&kTouchEvent);

  const ui::KeyEvent kKeyEvent(
      ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
      ui::DomKey::FromCharacter('a'), base::TimeTicks());
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);

  const ui::TouchEvent kStylusEvent(
      ui::ET_TOUCH_MOVED, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_PEN, 0),
      ui::EF_NONE);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  const ScreenBrightnessEvent::Features& features =
      info[0].screen_brightness_event.features();
  EXPECT_EQ(1, features.activity_data().num_recent_mouse_events());
  EXPECT_EQ(2, features.activity_data().num_recent_touch_events());
  EXPECT_EQ(3, features.activity_data().num_recent_key_events());
  EXPECT_EQ(4, features.activity_data().num_recent_stylus_events());
}

// Test is flaky. See https://crbug.com/938055.
TEST_F(AdaptiveScreenBrightnessManagerTest, DISABLED_SingleBrowser) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  BrowserList::GetInstance()->SetLastActive(browser.get());
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CreateTestWebContents(tab_strip_model, kUrl1, false /* is_active */);
  const ukm::SourceId source_id2 =
      CreateTestWebContents(tab_strip_model, kUrl2, true /* is_active */);

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_EQ(false, info[0].has_form_entry);

  // Browser DCHECKS that all tabs have been closed at destruction.
  tab_strip_model->CloseAllTabs();
}

// Test is flaky. See https://crbug.com/944325.
TEST_F(AdaptiveScreenBrightnessManagerTest,
       DISABLED_MultipleBrowsersWithActive) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized, so not visible.
  //  - browser2 and browser3 are both visible but browser2 is the topmost.

  std::unique_ptr<Browser> browser1 =
      CreateTestBrowser(false /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser2 =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  std::unique_ptr<Browser> browser3 =
      CreateTestBrowser(true /* is_visible */, false /* is_focused */);

  BrowserList::GetInstance()->SetLastActive(browser3.get());
  BrowserList::GetInstance()->SetLastActive(browser2.get());
  BrowserList::GetInstance()->SetLastActive(browser1.get());

  TabStripModel* tab_strip_model1 = browser1->tab_strip_model();
  CreateTestWebContents(tab_strip_model1, kUrl1, true /* is_active */);

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  const ukm::SourceId source_id2 =
      CreateTestWebContents(tab_strip_model2, kUrl2, true /* is_active */);

  TabStripModel* tab_strip_model3 = browser3->tab_strip_model();
  CreateTestWebContents(tab_strip_model3, kUrl3, true /* is_active */);

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_EQ(false, info[0].has_form_entry);

  // Browser DCHECKS that all tabs have been closed at destruction.
  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
  tab_strip_model3->CloseAllTabs();
}

TEST_F(AdaptiveScreenBrightnessManagerTest,
       DISABLED_MultipleBrowsersNoneActive) {
  // Simulates three browsers, none of which are active.
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 and browser3 are both visible but not focused so not active.
  //  - browser2 is the topmost.

  std::unique_ptr<Browser> browser1 =
      CreateTestBrowser(false /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser2 =
      CreateTestBrowser(true /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser3 =
      CreateTestBrowser(true /* is_visible */, false /* is_focused */);

  BrowserList::GetInstance()->SetLastActive(browser3.get());
  BrowserList::GetInstance()->SetLastActive(browser2.get());
  BrowserList::GetInstance()->SetLastActive(browser1.get());

  TabStripModel* tab_strip_model1 = browser1->tab_strip_model();
  CreateTestWebContents(tab_strip_model1, kUrl1, true /* is_active */);

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  const ukm::SourceId source_id2 =
      CreateTestWebContents(tab_strip_model2, kUrl2, true /* is_active */);

  TabStripModel* tab_strip_model3 = browser3->tab_strip_model();
  CreateTestWebContents(tab_strip_model3, kUrl3, true /* is_active */);

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_EQ(false, info[0].has_form_entry);

  // Browser DCHECKS that all tabs have been closed at destruction.
  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
  tab_strip_model3->CloseAllTabs();
}

TEST_F(AdaptiveScreenBrightnessManagerTest, BrowsersWithIncognito) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 is visible but not focused so not active.
  //  - browser3 is visible and focused, but incognito.

  std::unique_ptr<Browser> browser1 =
      CreateTestBrowser(false /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser2 =
      CreateTestBrowser(true /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser3 = CreateTestBrowser(
      true /* is_visible */, true /* is_focused */, true /* is_incognito */);

  BrowserList::GetInstance()->SetLastActive(browser3.get());
  BrowserList::GetInstance()->SetLastActive(browser2.get());
  BrowserList::GetInstance()->SetLastActive(browser1.get());

  TabStripModel* tab_strip_model1 = browser1->tab_strip_model();
  CreateTestWebContents(tab_strip_model1, kUrl1, true /* is_active */);

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  const ukm::SourceId source_id2 =
      CreateTestWebContents(tab_strip_model2, kUrl2, true /* is_active */);

  TabStripModel* tab_strip_model3 = browser3->tab_strip_model();
  CreateTestWebContents(tab_strip_model3, kUrl3, true /* is_active */);

  InitializeBrightness(75.0f);
  FireTimer();

  const std::vector<LogActivityInfo>& info = ukm_logger()->log_activity_info();
  ASSERT_EQ(1U, info.size());
  EXPECT_EQ(source_id2, info[0].tab_id);
  EXPECT_EQ(false, info[0].has_form_entry);

  // Browser DCHECKS that all tabs have been closed at destruction.
  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
  tab_strip_model3->CloseAllTabs();
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
