// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/smart_charging/smart_charging_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/user_charging_event.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace power {
namespace {

using PastEvent = power_manager::PastChargingEvents::Event;
using EventReason = power_manager::UserChargingEvent::Event::Reason;
using UserChargingEvent = power_manager::UserChargingEvent;
using ChargeHistoryState = power_manager::ChargeHistoryState;

PastEvent CreateEvent(int time,
                      int battery_percent,
                      int timezone,
                      const EventReason& reason) {
  PastEvent event;
  event.set_time(time);
  event.set_battery_percent(battery_percent);
  event.set_timezone(timezone);
  event.set_reason(reason);
  return event;
}
}  // namespace

class SmartChargingManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SmartChargingManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  ~SmartChargingManagerTest() override = default;
  SmartChargingManagerTest(const SmartChargingManagerTest&) = delete;
  SmartChargingManagerTest& operator=(const SmartChargingManagerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    chromeos::PowerManagerClient::InitializeFake();

    mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer;
    auto periodic_timer = std::make_unique<base::RepeatingTimer>();
    periodic_timer->SetTaskRunner(
        task_environment()->GetMainThreadTaskRunner());
    smart_charging_manager_ = std::make_unique<SmartChargingManager>(
        ui::UserActivityDetector::Get(),
        observer.InitWithNewPipeAndPassReceiver(), &session_manager_,
        std::move(periodic_timer));
  }

  void TearDown() override {
    smart_charging_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  UserChargingEvent GetUserChargingEvent() {
    return smart_charging_manager_->user_charging_event_for_test_;
  }

  std::optional<power_manager::PowerSupplyProperties::ExternalPower>
  GetExternalPower() {
    return smart_charging_manager_->external_power_;
  }

  void UpdateChargeHistory() { smart_charging_manager_->UpdateChargeHistory(); }

  ChargeHistoryState GetChargeHistory() {
    return smart_charging_manager_->charge_history_;
  }

 protected:
  void ReportUserActivity(const ui::Event* const event) {
    smart_charging_manager_->OnUserActivity(event);
  }

  void ReportPowerChangeEvent(
      const power_manager::PowerSupplyProperties::ExternalPower power,
      const float battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_external_power(power);
    proto.set_battery_percent(battery_percent);
    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
  }

  void ReportBrightnessChangeEvent(const double level) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(level);
    smart_charging_manager_->ScreenBrightnessChanged(change);
  }

  void ReportShutdownEvent() {
    smart_charging_manager_->ShutdownRequested(
        power_manager::RequestShutdownReason::REQUEST_SHUTDOWN_FOR_USER);
  }

  void ReportSuspendEvent() {
    smart_charging_manager_->SuspendImminent(
        power_manager::SuspendImminent::LID_CLOSED);
  }

  void ReportLidEvent(const chromeos::PowerManagerClient::LidState state) {
    chromeos::FakePowerManagerClient::Get()->SetLidState(
        state, base::TimeTicks::UnixEpoch());
  }

  void ReportTabletModeEvent(
      const chromeos::PowerManagerClient::TabletMode mode) {
    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        mode, base::TimeTicks::UnixEpoch());
  }

  void ReportVideoStart() { smart_charging_manager_->OnVideoActivityStarted(); }

  void ReportVideoEnd() { smart_charging_manager_->OnVideoActivityEnded(); }

  void FireTimer() { smart_charging_manager_->OnTimerFired(); }

  void InitializeBrightness(const double level) {
    smart_charging_manager_->OnReceiveScreenBrightnessPercent(level);
  }

  void FastForwardTimeBySecs(const int seconds) {
    task_environment()->FastForwardBy(base::Seconds(seconds));
  }

  base::TimeDelta GetDurationRecentVideoPlaying() {
    return smart_charging_manager_->DurationRecentVideoPlaying();
  }

  std::tuple<PastEvent, PastEvent> GetLastChargeEvents() {
    return smart_charging_manager_->GetLastChargeEvents();
  }

  void UpdatePastEvents() { smart_charging_manager_->UpdatePastEvents(); }

  std::vector<PastEvent> GetPastEvents() {
    return smart_charging_manager_->past_events_;
  }

  void SetBatteryPercentage(double battery_percent) {
    smart_charging_manager_->battery_percent_ = battery_percent;
  }

  void AddEvent(const PastEvent& event) {
    smart_charging_manager_->past_events_.emplace_back(event);
  }

  void AddPastEvent(const EventReason& reason) {
    smart_charging_manager_->AddPastEvent(reason);
  }

  void MaybeSaveToDisk(const base::FilePath& profile_path) {
    smart_charging_manager_->MaybeSaveToDisk(profile_path);
  }

  void MaybeLoadFromDisk(const base::FilePath& profile_path) {
    smart_charging_manager_->MaybeLoadFromDisk(profile_path);
  }

  void ClearPastEvents() { smart_charging_manager_->past_events_.clear(); }

  void Wait() { task_environment()->RunUntilIdle(); }

  const gfx::Point kEventLocation = gfx::Point(90, 90);
  const ui::MouseEvent kMouseEvent = ui::MouseEvent(ui::EventType::kMouseMoved,
                                                    kEventLocation,
                                                    kEventLocation,
                                                    base::TimeTicks(),
                                                    0,
                                                    0);

 private:
  session_manager::SessionManager session_manager_;
  std::unique_ptr<SmartChargingManager> smart_charging_manager_;
};

TEST_F(SmartChargingManagerTest, BrightnessRecoredCorrectly) {
  InitializeBrightness(55.3f);

  // LogEvent() hasn't been called yet.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         23.0f);
  EXPECT_FALSE(GetUserChargingEvent().has_features());
  EXPECT_FALSE(GetUserChargingEvent().has_event());

  ReportBrightnessChangeEvent(75.7f);

  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 15.0f);

  EXPECT_EQ(GetUserChargingEvent().features().screen_brightness_percent(), 75);
}

TEST_F(SmartChargingManagerTest, PowerChangedTest) {
  // LogEvent() hasn't been called yet.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         23.0f);
  EXPECT_EQ(GetExternalPower().value(),
            power_manager::PowerSupplyProperties::DISCONNECTED);
  EXPECT_FALSE(GetUserChargingEvent().has_features());
  EXPECT_FALSE(GetUserChargingEvent().has_event());

  // Plugs in the charger.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 15.0f);
  EXPECT_EQ(GetExternalPower().value(),
            power_manager::PowerSupplyProperties::AC);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 15);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 0);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::CHARGER_PLUGGED_IN);

  // Unplugs the charger.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::USB, 85.0f);
  EXPECT_EQ(GetExternalPower().value(),
            power_manager::PowerSupplyProperties::USB);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 85);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 1);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::CHARGER_UNPLUGGED);
}

TEST_F(SmartChargingManagerTest, TimerFiredTest) {
  // Timer fired 2 times.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::USB, 23.0f);
  FastForwardTimeBySecs(3600);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 23);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 1);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::PERIODIC_LOG);

  // Charger plugged in.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 50.0f);
  FastForwardTimeBySecs(1900);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 50);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 3);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::PERIODIC_LOG);
}

TEST_F(SmartChargingManagerTest, UserEventCounts) {
  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  const ui::TouchEvent kTouchEvent(
      ui::EventType::kTouchPressed, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ReportUserActivity(&kTouchEvent);
  ReportUserActivity(&kTouchEvent);

  const ui::KeyEvent kKeyEvent(
      ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A, 0,
      ui::DomKey::FromCharacter('a'), base::TimeTicks());
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);

  const ui::TouchEvent kStylusEvent(
      ui::EventType::kTouchMoved, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kPen, 0), ui::EF_NONE);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const auto& proto = GetUserChargingEvent();

  EXPECT_EQ(proto.features().num_recent_mouse_events(), 1);
  EXPECT_EQ(proto.features().num_recent_touch_events(), 2);
  EXPECT_EQ(proto.features().num_recent_key_events(), 3);
  EXPECT_EQ(proto.features().num_recent_stylus_events(), 4);
}

TEST_F(SmartChargingManagerTest, ShutdownAndSuspend) {
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         25.0f);
  // Suspend.
  ReportSuspendEvent();
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 25);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 0);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::SUSPEND);

  // Shutdown.
  ReportShutdownEvent();
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 25);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 1);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::SHUTDOWN);
}

TEST_F(SmartChargingManagerTest, VideoDuration) {
  // Video playing periods: [100, 200], [300, 500], [700, 800], [1500,2000].
  FastForwardTimeBySecs(100);
  ReportVideoStart();
  FastForwardTimeBySecs(100);
  ReportVideoEnd();
  FastForwardTimeBySecs(100);
  ReportVideoStart();
  FastForwardTimeBySecs(200);
  ReportVideoEnd();
  FastForwardTimeBySecs(200);
  ReportVideoStart();
  FastForwardTimeBySecs(100);
  ReportVideoEnd();
  FastForwardTimeBySecs(700);
  ReportVideoStart();
  FastForwardTimeBySecs(300);

  // At 1800s, total video playing time should be 700s.
  EXPECT_EQ(GetDurationRecentVideoPlaying().InSeconds(), 700);

  // At 2000s, total video playing time should be 800s.
  FastForwardTimeBySecs(200);
  ReportVideoEnd();
  EXPECT_EQ(GetDurationRecentVideoPlaying().InSeconds(), 800);

  // At 3600s, total video playing time should be 200s.
  FastForwardTimeBySecs(1600);
  EXPECT_EQ(GetDurationRecentVideoPlaying().InSeconds(), 200);
}

TEST_F(SmartChargingManagerTest, DeviceMode) {
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);

  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 15.0f);
  EXPECT_EQ(GetUserChargingEvent().features().device_mode(),
            UserChargingEvent::Features::LAPTOP_MODE);
}

TEST_F(SmartChargingManagerTest, GetLastChargeEventsNoLastCharges) {
  AddEvent(CreateEvent(1, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(2, 20, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(3, 30, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(
      CreateEvent(4, 40, 11, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(5, 50, 11, UserChargingEvent::Event::SHUTDOWN));
  AddEvent(CreateEvent(6, 60, 11, UserChargingEvent::Event::PERIODIC_LOG));

  auto [plugged_in, unplugged] = GetLastChargeEvents();
  EXPECT_FALSE(plugged_in.has_time());
  EXPECT_FALSE(unplugged.has_time());
}

TEST_F(SmartChargingManagerTest, GetLastChargeEventsComplex) {
  AddEvent(CreateEvent(1, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(2, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(3, 20, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(4, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(5, 30, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(
      CreateEvent(6, 20, 11, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(7, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(
      CreateEvent(8, 30, 11, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(9, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(10, 20, 1, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(11, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(12, 20, 1, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(13, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(
      CreateEvent(14, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(
      CreateEvent(15, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(16, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(17, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(18, 20, 1, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(19, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(
      CreateEvent(20, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(21, 40, 1, UserChargingEvent::Event::SHUTDOWN));
  AddEvent(
      CreateEvent(22, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));

  auto [plugged_in, unplugged] = GetLastChargeEvents();
  EXPECT_TRUE(plugged_in.has_time());
  EXPECT_TRUE(unplugged.has_time());
  EXPECT_EQ(plugged_in.time(), 15);
  EXPECT_EQ(unplugged.time(), 18);
}

TEST_F(SmartChargingManagerTest, UpdatePastEventsNoLastCharge) {
  AddEvent(CreateEvent(1, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(2, 20, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(3, 30, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(
      CreateEvent(4, 40, 11, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(5, 50, 11, UserChargingEvent::Event::SHUTDOWN));
  AddEvent(CreateEvent(6, 60, 11, UserChargingEvent::Event::PERIODIC_LOG));

  UpdatePastEvents();

  const std::vector<PastEvent> events = GetPastEvents();
  EXPECT_EQ(events.size(), static_cast<unsigned long>(2));
  EXPECT_EQ(events[0].time(), 4);
  EXPECT_EQ(events[1].time(), 5);
}

TEST_F(SmartChargingManagerTest, UpdatePastEventsComplex) {
  AddEvent(CreateEvent(1, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(2, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(3, 20, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(4, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(5, 30, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(
      CreateEvent(6, 20, 11, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(7, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(
      CreateEvent(8, 30, 11, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(9, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(10, 20, 1, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(11, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(12, 20, 1, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(13, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(
      CreateEvent(14, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(
      CreateEvent(15, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(16, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(17, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(18, 20, 1, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(19, 10, 1, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(
      CreateEvent(20, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));
  AddEvent(CreateEvent(21, 40, 1, UserChargingEvent::Event::SHUTDOWN));
  AddEvent(
      CreateEvent(22, 40, 1, UserChargingEvent::Event::CHARGER_PLUGGED_IN));

  UpdatePastEvents();

  const std::vector<PastEvent> events = GetPastEvents();
  EXPECT_EQ(events.size(), static_cast<unsigned long>(4));
  EXPECT_EQ(events[0].time(), 15);
  EXPECT_EQ(events[1].time(), 18);
  EXPECT_EQ(events[2].time(), 22);
  EXPECT_EQ(events[3].time(), 21);
}

TEST_F(SmartChargingManagerTest, AddPastEventTest) {
  SetBatteryPercentage(15.5);
  AddPastEvent(UserChargingEvent::Event::CHARGER_PLUGGED_IN);
  SetBatteryPercentage(25.7);
  AddPastEvent(UserChargingEvent::Event::CHARGER_UNPLUGGED);

  const std::vector<PastEvent> events = GetPastEvents();
  EXPECT_EQ(events.size(), static_cast<unsigned long>(2));
  EXPECT_EQ(events[0].battery_percent(), 15);
  EXPECT_EQ(events[0].reason(), UserChargingEvent::Event::CHARGER_PLUGGED_IN);
  EXPECT_EQ(events[1].battery_percent(), 25);
  EXPECT_EQ(events[1].reason(), UserChargingEvent::Event::CHARGER_UNPLUGGED);
}

TEST_F(SmartChargingManagerTest, LoadAndSave) {
  AddEvent(CreateEvent(1, 10, 11, UserChargingEvent::Event::PERIODIC_LOG));
  AddEvent(CreateEvent(2, 20, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));
  AddEvent(CreateEvent(3, 30, 11, UserChargingEvent::Event::CHARGER_UNPLUGGED));

  EXPECT_EQ(GetPastEvents().size(), static_cast<unsigned long>(3));

  // Save to disk
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir.GetPath().AppendASCII("smartcharging")));
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("smartcharging/past_charging_events.pb");

  MaybeSaveToDisk(temp_dir.GetPath());
  Wait();
  ASSERT_TRUE(base::PathExists(file_path));

  // Clear memory
  ClearPastEvents();

  // Now there is no past event on memory
  EXPECT_EQ(GetPastEvents().size(), static_cast<unsigned long>(0));

  // Load from disk
  MaybeLoadFromDisk(temp_dir.GetPath());
  Wait();

  // Check the result
  const std::vector<PastEvent> events = GetPastEvents();
  EXPECT_EQ(events.size(), static_cast<unsigned long>(3));
  EXPECT_EQ(events[0].time(), 1);
  EXPECT_EQ(events[1].time(), 2);
  EXPECT_EQ(events[2].time(), 3);
}

TEST_F(SmartChargingManagerTest, LastChargeRelatedFeatures) {
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  FastForwardTimeBySecs(3600);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         80.0f);
  FastForwardTimeBySecs(600);
  ReportShutdownEvent();
  FastForwardTimeBySecs(1800);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 75.0f);

  const auto features = GetUserChargingEvent().features();

  EXPECT_TRUE(features.halt_from_last_charge());
  EXPECT_EQ(features.time_since_last_charge_minutes(), 38);
  EXPECT_EQ(features.duration_of_last_charge_minutes(), 58);
  EXPECT_EQ(features.battery_percentage_before_last_charge(), 23);
  EXPECT_EQ(features.battery_percentage_of_last_charge(), 80);
}

TEST_F(SmartChargingManagerTest, GetCorrectChargeHistory) {
  ChargeHistoryState charge_history;
  ChargeHistoryState::ChargeEvent charge_event;
  ChargeHistoryState::DailyHistory daily_history;

  charge_event.set_start_time(1234);
  daily_history.set_utc_midnight(3456);

  for (size_t i = 0; i < 50; ++i)
    *charge_history.add_charge_event() = charge_event;
  for (size_t i = 0; i < 30; ++i)
    *charge_history.add_daily_history() = daily_history;

  chromeos::FakePowerManagerClient::Get()->SetChargeHistoryForAdaptiveCharging(
      charge_history);

  UpdateChargeHistory();
  Wait();

  ChargeHistoryState charge_history_obtained = GetChargeHistory();
  EXPECT_EQ(charge_history_obtained.charge_event_size(), 50);
  EXPECT_TRUE(charge_history_obtained.charge_event(10).has_start_time());
  EXPECT_EQ(charge_history_obtained.charge_event(10).start_time(), 1234);
  EXPECT_EQ(charge_history_obtained.daily_history_size(), 30);
  EXPECT_TRUE(charge_history_obtained.daily_history(10).has_utc_midnight());
  EXPECT_EQ(charge_history_obtained.daily_history(10).utc_midnight(), 3456);
}

}  // namespace power
}  // namespace ash
