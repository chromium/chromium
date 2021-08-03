// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/machine_learning/user_settings_event_logger.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/system/machine_learning/user_settings_event.pb.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {
namespace ml {
namespace {

using chromeos::network_config::mojom::CellularStateProperties;
using chromeos::network_config::mojom::CellularStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::NetworkTypeStateProperties;
using chromeos::network_config::mojom::SecurityType;
using chromeos::network_config::mojom::WiFiStateProperties;
using chromeos::network_config::mojom::WiFiStatePropertiesPtr;
using display::Display;
using power_manager::PowerSupplyProperties;
using ukm::TestUkmRecorder;

NetworkStatePropertiesPtr CreateWifiNetwork(int signal_strength,
                                            SecurityType security_type) {
  NetworkStatePropertiesPtr network = NetworkStateProperties::New();

  WiFiStatePropertiesPtr wifi = WiFiStateProperties::New();
  wifi->signal_strength = signal_strength;
  wifi->security = security_type;
  network->type = NetworkType::kWiFi;
  network->type_state = NetworkTypeStateProperties::New();
  network->type_state->set_wifi(std::move(wifi));

  return network;
}

NetworkStatePropertiesPtr CreateCellularNetwork(int signal_strength) {
  NetworkStatePropertiesPtr network = NetworkStateProperties::New();

  CellularStatePropertiesPtr cellular = CellularStateProperties::New();
  cellular->signal_strength = signal_strength;
  network->type = NetworkType::kCellular;
  network->type_state = NetworkTypeStateProperties::New();
  network->type_state->set_cellular(std::move(cellular));

  return network;
}

class FakeNightLightDelegate : public NightLightControllerImpl::Delegate {
 public:
  FakeNightLightDelegate() = default;
  ~FakeNightLightDelegate() override = default;
  FakeNightLightDelegate(const FakeNightLightDelegate&) = delete;
  FakeNightLightDelegate& operator=(const FakeNightLightDelegate&) = delete;

  void SetFakeNow(TimeOfDay time) { fake_now_ = time.ToTimeToday(); }
  void SetFakeSunset(TimeOfDay time) { fake_sunset_ = time.ToTimeToday(); }
  void SetFakeSunrise(TimeOfDay time) { fake_sunrise_ = time.ToTimeToday(); }

  // NightLightControllerImpl::Delegate:
  base::Time GetNow() const override { return fake_now_; }
  base::Time GetSunsetTime() const override { return fake_sunset_; }
  base::Time GetSunriseTime() const override { return fake_sunrise_; }
  bool SetGeoposition(
      const NightLightController::SimpleGeoposition& position) override {
    return false;
  }
  bool HasGeoposition() const override { return false; }

 private:
  base::Time fake_now_;
  base::Time fake_sunset_;
  base::Time fake_sunrise_;
};

}  // namespace

class UserSettingsEventLoggerTest : public AshTestBase {
 public:
  UserSettingsEventLoggerTest()
      : AshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  ~UserSettingsEventLoggerTest() override = default;
  UserSettingsEventLoggerTest(const UserSettingsEventLoggerTest&) = delete;
  UserSettingsEventLoggerTest& operator=(const UserSettingsEventLoggerTest&) =
      delete;

  void SetUp() override {
    AshTestBase::SetUp();
    logger_ = UserSettingsEventLogger::Get();
  }

 protected:
  void FastForwardBySeconds(const int seconds) {
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(seconds));
  }

  std::vector<const ukm::mojom::UkmEntry*> GetUkmEntries() {
    return ukm_recorder_.GetEntriesByName(
        ukm::builders::UserSettingsEvent::kEntryName);
  }

  // Volume features are logged at the end of the timer delay.
  void LogVolumeAndWait(const int volume) {
    logger_->OnOutputNodeVolumeChanged(0, volume);
    task_environment()->FastForwardBy(kSliderDelay);
  }

  void SetMuteAndWait(const bool mute_on) {
    logger_->OnOutputMuteChanged(mute_on);
    task_environment()->FastForwardBy(kSliderDelay);
  }

  void LogBrightness(const int brightness,
                     const bool initiated_by_user = true) {
    power_manager::BacklightBrightnessChange change;
    change.set_cause(
        initiated_by_user
            ? power_manager::BacklightBrightnessChange_Cause_USER_REQUEST
            : power_manager::BacklightBrightnessChange_Cause_OTHER);
    change.set_percent(brightness);

    logger_->ScreenBrightnessChanged(change);
  }

  // Brightness features are logged at the end of the timer delay.
  void LogBrightnessAndWait(const int current_level,
                            const bool initiated_by_user = true) {
    LogBrightness(current_level, initiated_by_user);
    task_environment()->FastForwardBy(kSliderDelay);
  }

  void LogSharedFeatures() {
    UserSettingsEvent event;
    logger_->PopulateSharedFeatures(&event);
    logger_->SendToUkmAndAppList(event);
  }

  UserSettingsEventLogger* logger_;

 private:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

TEST_F(UserSettingsEventLoggerTest, TestLogWifiEvent) {
  logger_->LogNetworkUkmEvent(
      *CreateWifiNetwork(50, SecurityType::kNone).get());
  logger_->LogNetworkUkmEvent(
      *CreateWifiNetwork(25, SecurityType::kWpaEap).get());

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::WIFI);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 50);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasWifiSecurity", false);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::WIFI);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 25);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasWifiSecurity", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogCellularEvent) {
  logger_->LogNetworkUkmEvent(*CreateCellularNetwork(50).get());
  logger_->LogNetworkUkmEvent(*CreateCellularNetwork(25).get());

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::CELLULAR);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 50);
  TestUkmRecorder::ExpectEntryMetric(entry, "UsedCellularInSession", false);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::CELLULAR);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 25);
  // After the first cellular event, |UsedCellularInSession| should now be true.
  TestUkmRecorder::ExpectEntryMetric(entry, "UsedCellularInSession", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogBluetoothEvent) {
  // Log an event with a null bluetooth address.
  logger_->LogBluetoothUkmEvent(BluetoothAddress());

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(1ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::BLUETOOTH);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  // |is_paired_bluetooth_device| is tested manually.
}

TEST_F(UserSettingsEventLoggerTest, TestLogNightLightEvent) {
  Shell::Get()->night_light_controller()->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);
  logger_->LogNightLightUkmEvent(true);

  Shell::Get()->night_light_controller()->SetScheduleType(
      NightLightController::ScheduleType::kNone);
  logger_->LogNightLightUkmEvent(false);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::NIGHT_LIGHT);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasNightLightSchedule", true);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::NIGHT_LIGHT);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasNightLightSchedule", false);
}

TEST_F(UserSettingsEventLoggerTest, TestNightLightSunsetFeature) {
  auto night_light_delegate = std::make_unique<FakeNightLightDelegate>();
  auto* night_light = night_light_delegate.get();
  Shell::Get()->night_light_controller()->SetDelegateForTesting(
      std::move(night_light_delegate));

  // Set fake sunrise and sunset to 6am and 6pm.
  night_light->SetFakeSunrise(TimeOfDay(6 * 60));
  night_light->SetFakeSunset(TimeOfDay(18 * 60));

  // Log events at 3am, 12pm, and 11pm.
  night_light->SetFakeNow(TimeOfDay(3 * 60));
  logger_->LogNightLightUkmEvent(false);
  night_light->SetFakeNow(TimeOfDay(12 * 60));
  logger_->LogNightLightUkmEvent(false);
  night_light->SetFakeNow(TimeOfDay(23 * 60));
  logger_->LogNightLightUkmEvent(false);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(3ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "IsAfterSunset", true);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsAfterSunset", false);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsAfterSunset", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogQuietModeEvent) {
  // Log an event that quiet mode has been switched on.
  logger_->LogQuietModeUkmEvent(true);

  // Start two casting sessions.
  logger_->OnCastingSessionStartedOrStopped(true);
  logger_->OnCastingSessionStartedOrStopped(true);
  logger_->LogQuietModeUkmEvent(false);

  // End one casting session.
  logger_->OnCastingSessionStartedOrStopped(false);
  FastForwardBySeconds(301);
  logger_->LogQuietModeUkmEvent(false);

  // End the final casting session. |is_recently_presenting| should remain true
  // for 5 minutes, with 1 second given for leeway on either side.
  logger_->OnCastingSessionStartedOrStopped(false);
  FastForwardBySeconds(299);
  logger_->LogQuietModeUkmEvent(false);
  FastForwardBySeconds(2);
  logger_->LogQuietModeUkmEvent(false);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(5ul, entries.size());

  // Check that the first entry has all details recorded correctly.
  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::DO_NOT_DISTURB);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsRecentlyPresenting", false);

  // Check that subsequent entries correctly record |is_recently_presenting|.
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsRecentlyPresenting", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsRecentlyPresenting", true);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "IsRecentlyPresenting", true);
  TestUkmRecorder::ExpectEntryMetric(entries[4], "IsRecentlyPresenting", false);
}

TEST_F(UserSettingsEventLoggerTest, TestLogAccessibilityEvent) {
  // Log an event that high contrast mode has been enabled.
  logger_->LogAccessibilityUkmEvent(UserSettingsEvent::Event::HIGH_CONTRAST,
                                    true);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(1ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::ACCESSIBILITY);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "AccessibilityId",
                                     UserSettingsEvent::Event::HIGH_CONTRAST);
}

TEST_F(UserSettingsEventLoggerTest, TestLogVolumeFeatures) {
  LogVolumeAndWait(23);
  LogVolumeAndWait(98);
  SetMuteAndWait(true);
  SetMuteAndWait(false);
  LogVolumeAndWait(20);
  SetMuteAndWait(true);
  LogVolumeAndWait(50);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(7ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::VOLUME);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 23);

  TestUkmRecorder::ExpectEntryMetric(entries[1], "PreviousValue", 23);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "CurrentValue", 98);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "PreviousValue", 98);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "CurrentValue", 0);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "PreviousValue", 0);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "CurrentValue", 98);
  TestUkmRecorder::ExpectEntryMetric(entries[4], "PreviousValue", 98);
  TestUkmRecorder::ExpectEntryMetric(entries[4], "CurrentValue", 20);
  TestUkmRecorder::ExpectEntryMetric(entries[5], "PreviousValue", 20);
  TestUkmRecorder::ExpectEntryMetric(entries[5], "CurrentValue", 0);
  TestUkmRecorder::ExpectEntryMetric(entries[6], "PreviousValue", 0);
  TestUkmRecorder::ExpectEntryMetric(entries[6], "CurrentValue", 50);
}

TEST_F(UserSettingsEventLoggerTest, TestVolumeDelay) {
  LogVolumeAndWait(10);

  // Only log an event if there is a pause of |kSliderDelay|.
  logger_->OnOutputNodeVolumeChanged(0, 11);
  task_environment()->FastForwardBy(kSliderDelay / 2);
  logger_->OnOutputNodeVolumeChanged(0, 12);
  task_environment()->FastForwardBy(kSliderDelay / 2);
  logger_->OnOutputNodeVolumeChanged(0, 13);
  task_environment()->FastForwardBy(kSliderDelay);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "CurrentValue", 10);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "PreviousValue", 10);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "CurrentValue", 13);
}

TEST_F(UserSettingsEventLoggerTest, TestLogBrightnessReason) {
  LogBrightnessAndWait(1);
  LogBrightnessAndWait(2);
  LogBrightnessAndWait(3, false);  // Not logged, but updates the brightness.
  LogBrightnessAndWait(4);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(3ul, entries.size());

  // Check that the first entry has basic details recorded correctly.
  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::BRIGHTNESS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  EXPECT_FALSE(TestUkmRecorder::EntryHasMetric(entry, "PreviousValue"));
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 1);

  // Check that subsequent entries correctly record values.
  TestUkmRecorder::ExpectEntryMetric(entries[1], "PreviousValue", 1);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "CurrentValue", 2);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "PreviousValue", 3);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "CurrentValue", 4);
}

TEST_F(UserSettingsEventLoggerTest, TestLogRecentlyFullscreen) {
  LogBrightnessAndWait(0);

  // Enter fullscreen.
  logger_->OnFullscreenStateChanged(true, nullptr);
  LogBrightnessAndWait(0);

  // Exit fullscreen. |is_recently_fullscreen| should remain true for 5 minutes,
  // with 1 second given for leeway on either side.
  logger_->OnFullscreenStateChanged(false, nullptr);
  FastForwardBySeconds(299 - kSliderDelay.InSeconds());
  LogBrightnessAndWait(0);
  FastForwardBySeconds(2);
  LogBrightnessAndWait(0);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(4ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "IsRecentlyFullscreen", false);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsRecentlyFullscreen", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsRecentlyFullscreen", true);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "IsRecentlyFullscreen", false);
}

TEST_F(UserSettingsEventLoggerTest, TestBrightnessDelay) {
  LogBrightnessAndWait(10, false);  // Set an initial unlogged value.

  // Only log an event if there is a pause of |kSliderDelay|.
  LogBrightness(11);
  task_environment()->FastForwardBy(kSliderDelay / 2);
  LogBrightness(12);
  task_environment()->FastForwardBy(kSliderDelay / 2);
  LogBrightness(13);
  LogBrightness(14, false);
  LogBrightness(15);
  LogBrightness(16, false);
  task_environment()->FastForwardBy(kSliderDelay);

  LogBrightnessAndWait(17);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::BRIGHTNESS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", 10);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 15);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", 16);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 17);
}

TEST_F(UserSettingsEventLoggerTest, TestLogDatetimeFeatures) {
  base::SimpleTestClock test_clock;
  logger_->SetClockForTesting(&test_clock);
  base::Time fake_time;

  ASSERT_TRUE(base::Time::FromString("Thu, 7 Nov 2019 14:58:00", &fake_time));
  test_clock.SetNow(fake_time);
  LogSharedFeatures();

  ASSERT_TRUE(base::Time::FromString("Fri, 8 Nov 2019 04:24:00", &fake_time));
  test_clock.SetNow(fake_time);
  LogSharedFeatures();

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "HourOfDay", 14);
  TestUkmRecorder::ExpectEntryMetric(entry, "DayOfWeek",
                                     UserSettingsEvent::Features::THU);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "HourOfDay", 4);
  TestUkmRecorder::ExpectEntryMetric(entry, "DayOfWeek",
                                     UserSettingsEvent::Features::FRI);
}

TEST_F(UserSettingsEventLoggerTest, TestLogPowerFeatures) {
  PowerSupplyProperties fake_power;

  fake_power.set_battery_percent(56.0);
  fake_power.set_external_power(PowerSupplyProperties::DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(fake_power);
  LogSharedFeatures();

  fake_power.set_external_power(PowerSupplyProperties::AC);
  PowerStatus::Get()->SetProtoForTesting(fake_power);
  LogSharedFeatures();

  fake_power.set_external_power(PowerSupplyProperties::USB);
  PowerStatus::Get()->SetProtoForTesting(fake_power);
  LogSharedFeatures();

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(3ul, entries.size());

  auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "BatteryPercentage", 56);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsCharging", false);

  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsCharging", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsCharging", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogAudio) {
  LogSharedFeatures();
  logger_->OnOutputStarted();
  LogSharedFeatures();
  logger_->OnOutputStopped();
  LogSharedFeatures();

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(3ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "IsPlayingAudio", false);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsPlayingAudio", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsPlayingAudio", false);
}

TEST_F(UserSettingsEventLoggerTest, TestLogVideo) {
  LogSharedFeatures();
  logger_->OnVideoStateChanged(VideoDetector::State::PLAYING_FULLSCREEN);
  LogSharedFeatures();
  logger_->OnVideoStateChanged(VideoDetector::State::NOT_PLAYING);
  LogSharedFeatures();
  logger_->OnVideoStateChanged(VideoDetector::State::PLAYING_WINDOWED);
  LogSharedFeatures();

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(4ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "IsPlayingVideo", false);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsPlayingVideo", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsPlayingVideo", false);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "IsPlayingVideo", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogTabletMode) {
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();

  tablet_mode_controller->SetEnabledForTest(true);
  LogSharedFeatures();

  tablet_mode_controller->SetEnabledForTest(false);
  LogSharedFeatures();

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "DeviceMode",
                                     UserSettingsEvent::Features::TABLET_MODE);
  TestUkmRecorder::ExpectEntryMetric(
      entries[1], "DeviceMode", UserSettingsEvent::Features::CLAMSHELL_MODE);
}

TEST_F(UserSettingsEventLoggerTest, TestLogOrientation) {
  display::test::ScopedSetInternalDisplayId set_internal_display(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());

  orientation_test_api.SetDisplayRotation(Display::ROTATE_0,
                                          Display::RotationSource::ACTIVE);
  LogSharedFeatures();

  orientation_test_api.SetDisplayRotation(Display::ROTATE_90,
                                          Display::RotationSource::ACTIVE);
  LogSharedFeatures();

  orientation_test_api.SetDisplayRotation(Display::ROTATE_180,
                                          Display::RotationSource::ACTIVE);
  LogSharedFeatures();

  orientation_test_api.SetDisplayRotation(Display::ROTATE_270,
                                          Display::RotationSource::ACTIVE);
  LogSharedFeatures();

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(4ul, entries.size());

  TestUkmRecorder::ExpectEntryMetric(entries[0], "DeviceOrientation",
                                     UserSettingsEvent::Features::LANDSCAPE);
  TestUkmRecorder::ExpectEntryMetric(entries[1], "DeviceOrientation",
                                     UserSettingsEvent::Features::PORTRAIT);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "DeviceOrientation",
                                     UserSettingsEvent::Features::LANDSCAPE);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "DeviceOrientation",
                                     UserSettingsEvent::Features::PORTRAIT);
}

}  // namespace ml
}  // namespace ash
