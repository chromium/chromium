// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller_test_util.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@geolocation";
constexpr char kUser2Email[] = "user2@geolocation";

// Sets of test longitudes/latitude and the corresponding sunrise/sunset times
// for testing. They all assume the clock's current time is `kTestNow`.
constexpr base::StringPiece kTestNow = "23 Dec 2021 12:00:00";

constexpr double kTestLatitude1 = 23.5;
constexpr double kTestLongitude1 = 35.88;
constexpr base::StringPiece kTestSunriseTime1 = "23 Dec 2021 04:14:36.626";
constexpr base::StringPiece kTestSunsetTime1 = "23 Dec 2021 14:59:58.459";

constexpr double kTestLatitude2 = 37.5;
constexpr double kTestLongitude2 = -100.5;
constexpr base::StringPiece kTestSunriseTime2 = "23 Dec 2021 13:55:13.306";
constexpr base::StringPiece kTestSunsetTime2 = "23 Dec 2021 23:33:46.855";

constexpr int kDefaultSunsetTimeOffsetMinutes = 18 * 60;
constexpr int kDefaultSunriseTimeOffsetMinutes = 6 * 60;

// Constructs a TimeZone object from the given `timezone_id`.
std::unique_ptr<icu::TimeZone> CreateTimezone(const char* timezone_id) {
  return base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id, -1, US_INV)));
}

std::u16string GetTimezoneId(const icu::TimeZone& timezone) {
  return system::TimezoneSettings::GetTimezoneID(timezone);
}

base::Time ToUTCTime(base::StringPiece utc_time_str) {
  base::Time time;
  CHECK(base::Time::FromUTCString(std::string(utc_time_str).c_str(), &time))
      << "Invalid UTC time string specified: " << utc_time_str;
  return time;
}

class FakeGeolocationController : public GeolocationController {
 public:
  explicit FakeGeolocationController(
      scoped_refptr<network::SharedURLLoaderFactory> factory)
      : GeolocationController(factory) {}

  // Proxy method to call the `OnGeoposition()` callback directly, without
  // waiting for the server response. Need this to test scheduler behavior.
  void ImitateGeopositionReceived() {
    Geoposition fake_pos;
    fake_pos.latitude = kTestLatitude1;
    fake_pos.longitude = kTestLongitude1;
    fake_pos.status = Geoposition::STATUS_OK;
    fake_pos.accuracy = 10;
    fake_pos.timestamp = base::Time::Now();

    GeolocationController::OnGeoposition(fake_pos, false, base::Seconds(1));
  }

  // TODO(b/286233027): Override `RequestGeolocation()` to fake the server
  // communication.
};

// Base test fixture.
class GeolocationControllerTest : public AshTestBase {
 public:
  GeolocationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  GeolocationControllerTest(const GeolocationControllerTest&) = delete;
  GeolocationControllerTest& operator=(const GeolocationControllerTest&) =
      delete;

  ~GeolocationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    CreateTestUserSessions();
    controller_ = std::make_unique<FakeGeolocationController>(
        static_cast<scoped_refptr<network::SharedURLLoaderFactory>>(
            base::MakeRefCounted<TestGeolocationUrlLoaderFactory>()));

    test_clock_.SetNow(base::Time::Now());
    controller_->SetClockForTesting(&test_clock_);
    timer_ptr_ = controller_->GetTimerForTesting();

    // Prepare a valid geoposition.
    Geoposition position;
    position.latitude = 32.0;
    position.longitude = 31.0;
    position.status = Geoposition::STATUS_OK;
    position.accuracy = 10;
    position.timestamp = base::Time::Now();
    SetServerPosition(position);
  }

  // AshTestBase:
  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

  FakeGeolocationController* controller() const { return controller_.get(); }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  base::OneShotTimer* timer_ptr() const { return timer_ptr_; }
  const Geoposition& position() const { return position_; }

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  // Fires the timer of the scheduler to request geoposition and wait for all
  // observers to receive the latest geoposition from the server.
  void FireTimerToFetchGeoposition() {
    GeopositionResponsesWaiter waiter(controller());
    EXPECT_TRUE(timer_ptr()->IsRunning());
    // Fast forward the scheduler to reach the time when the controller
    // requests for geoposition from the server in
    // `GeolocationController::RequestGeoposition`.
    timer_ptr_->FireNow();
    // Waits for the observers to receive the geoposition from the server.
    waiter.Wait();
  }

  // Sets the geoposition to be returned from the `factory_` upon the
  // `GeolocationController` request.
  void SetServerPosition(const Geoposition& position) {
    position_ = position;
    auto* factory = static_cast<TestGeolocationUrlLoaderFactory*>(
        controller_->GetSharedURLLoaderFactoryForTesting());
    factory->ClearResponses();
    factory->set_position(position_);
  }

  void UpdateUserGeolocationPermission(bool enabled) {
    PrefService* pref_service =
        Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    pref_service->SetBoolean(ash::prefs::kUserGeolocationAllowed, enabled);
  }

 private:
  std::unique_ptr<FakeGeolocationController> controller_;
  base::SimpleTestClock test_clock_;
  raw_ptr<base::OneShotTimer, ExperimentalAsh> timer_ptr_;
  Geoposition position_;
};

// Tests adding and removing an observer should request and stop receiving
// a position update.
TEST_F(GeolocationControllerTest, Observer) {
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Add an observer should start the timer requesting the geoposition.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer.position_received_num());

  // Check that the timer fires another schedule after a successful request.
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // Removing an observer should stop the timer.
  controller()->RemoveObserver(&observer);
  EXPECT_FALSE(timer_ptr()->IsRunning());
  EXPECT_EQ(1, observer.position_received_num());
}

// Tests adding and removing observer and make sure that only observing ones
// receive the position updates.
TEST_F(GeolocationControllerTest, MultipleObservers) {
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Add an observer should start the timer requesting for the first
  // geoposition request.
  GeolocationControllerObserver observer1;
  controller()->AddObserver(&observer1);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer1.position_received_num());
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // Since `OnGeoposition()` handling a geoposition update always schedule
  // the next geoposition request, the timer should keep running and
  // update position periodically.
  FireTimerToFetchGeoposition();
  EXPECT_EQ(2, observer1.position_received_num());
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // Adding `observer2` should not interrupt the request flow. Check that both
  // observers receive the new position.
  GeolocationControllerObserver observer2;
  controller()->AddObserver(&observer2);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(1, observer2.position_received_num());
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // Remove `observer1` and make sure that the timer is still running.
  // Only `observer2` should receive the new position.
  controller()->RemoveObserver(&observer1);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(2, observer2.position_received_num());
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // Removing `observer2` should stop the timer. The request count should
  // not change.
  controller()->RemoveObserver(&observer2);
  EXPECT_FALSE(timer_ptr()->IsRunning());
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(2, observer2.position_received_num());
}

// Tests that controller only pushes valid positions.
TEST_F(GeolocationControllerTest, InvalidPositions) {
  GeolocationControllerObserver observer;
  // Update to an invalid position
  Geoposition invalid_position(position());
  invalid_position.error_code = 10;
  SetServerPosition(invalid_position);
  EXPECT_FALSE(timer_ptr()->IsRunning());
  controller()->AddObserver(&observer);

  // If the position is invalid, the controller won't push the geoposition
  // update to its observers.
  EXPECT_TRUE(timer_ptr()->IsRunning());
  timer_ptr()->FireNow();
  // Wait for the request and response to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer.position_received_num());
  // With error response, the server will retry with another timer which we
  // have no control over, so `mock_timer_ptr_` will not be running (refers to
  // `SimpleGeolocationRequest::Retry()` for more detail).
  EXPECT_FALSE(timer_ptr()->IsRunning());
}

// Tests that timezone changes result.
TEST_F(GeolocationControllerTest, TimezoneChanges) {
  EXPECT_FALSE(timer_ptr()->IsRunning());
  controller()->SetCurrentTimezoneIdForTesting(u"America/Los_Angeles");

  // Add an observer.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);
  EXPECT_EQ(0, observer.position_received_num());
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer.position_received_num());
  EXPECT_EQ(u"America/Los_Angeles", controller()->current_timezone_id());
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // A new timezone results in new geoposition request.
  auto timezone = CreateTimezone("Asia/Tokyo");

  controller()->TimezoneChanged(*timezone);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(2, observer.position_received_num());
  EXPECT_EQ(GetTimezoneId(*timezone), controller()->current_timezone_id());
  EXPECT_TRUE(timer_ptr()->IsRunning());
}

TEST_F(GeolocationControllerTest, SystemGeolocationPermissionChanges) {
  EXPECT_FALSE(timer_ptr()->IsRunning());

  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);
  EXPECT_EQ(0, observer.position_received_num());

  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer.position_received_num());
  EXPECT_TRUE(timer_ptr()->IsRunning());

  // Disable system geo permission. Scheduling should stop.
  controller()->OnSystemGeolocationPermissionChanged(false);
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Re-enabling the system geo permission, should resume scheduling.
  controller()->OnSystemGeolocationPermissionChanged(true);
  EXPECT_TRUE(timer_ptr()->IsRunning());
}

TEST_F(GeolocationControllerTest, StopSchedulingWhileResponseIsComing) {
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // This will start scheduling.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);

  // Fire Geolocation request.
  timer_ptr()->FireNow();
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Disable user geolocation permission, this should stop scheduling.
  UpdateUserGeolocationPermission(false);
  controller()->OnSystemGeolocationPermissionChanged(false);
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Simulate server response and check it didn't trigger scheduling to
  // continue.
  controller()->ImitateGeopositionReceived();
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Re-enable user geolocation permission, this should resume scheduling.
  UpdateUserGeolocationPermission(true);
  controller()->OnSystemGeolocationPermissionChanged(true);
  EXPECT_TRUE(timer_ptr()->IsRunning());
}

TEST_F(GeolocationControllerTest, StopSchedulingWhenObserverListIsEmpty) {
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Add the first observer. This will kick off scheduling.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);

  // Fire Geolocation request.
  timer_ptr()->FireNow();

  // Unsubscribe the only observer.
  controller()->RemoveObserver(&observer);

  // Simulate the server response and check it didn't resume the scheduler.
  controller()->ImitateGeopositionReceived();
  EXPECT_FALSE(timer_ptr()->IsRunning());

  // Add the observer back, scheduling should resume.
  controller()->AddObserver(&observer);
  EXPECT_TRUE(timer_ptr()->IsRunning());
}

// Tests obtaining sunset/sunrise time when there is no valid geoposition, for
// example, due to lack of connectivity.
TEST_F(GeolocationControllerTest, SunsetSunriseDefault) {
  // If geoposition is unset, the controller should return the default sunset
  // and sunrise time .
  EXPECT_EQ(controller()->GetSunsetTime(),
            TimeOfDay(kDefaultSunsetTimeOffsetMinutes).ToTimeToday());
  EXPECT_EQ(controller()->GetSunriseTime(),
            TimeOfDay(kDefaultSunriseTimeOffsetMinutes).ToTimeToday());
}

// Tests the behavior when there is a valid geoposition, sunrise and sunset
// times are calculated correctly.
TEST_F(GeolocationControllerTest, GetSunRiseSet) {
  test_clock()->SetNow(ToUTCTime(kTestNow));

  // Add an observer and make sure that sunset and sunrise time are not
  // updated until the timer is fired.
  GeolocationControllerObserver observer1;
  controller()->AddObserver(&observer1);
  EXPECT_TRUE(timer_ptr()->IsRunning());
  EXPECT_NE(controller()->GetSunsetTime(), ToUTCTime(kTestSunsetTime1));
  EXPECT_NE(controller()->GetSunriseTime(), ToUTCTime(kTestSunriseTime1));
  EXPECT_EQ(0, observer1.position_received_num());

  // Prepare a valid geoposition.
  Geoposition position;
  position.latitude = kTestLatitude1;
  position.longitude = kTestLongitude1;
  position.status = Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = ToUTCTime(kTestNow);

  // Test that after sending the new position, sunrise and sunset time are
  // updated correctly.
  SetServerPosition(position);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer1.position_received_num());
  EXPECT_EQ(controller()->GetSunsetTime(), ToUTCTime(kTestSunsetTime1));
  EXPECT_EQ(controller()->GetSunriseTime(), ToUTCTime(kTestSunriseTime1));
  EXPECT_TRUE(timer_ptr()->IsRunning());
}

// Tests that when there is a geoposition with 24 hours of daylight or darkness,
// sunrise and sunset times honor the API.
TEST_F(GeolocationControllerTest, GetSunRiseSetWithAllDaylightOrDarkness) {
  static constexpr base::StringPiece kNow = "07 Jun 2023 20:30:00.000";
  test_clock()->SetNow(ToUTCTime(kNow));

  // 24 hours of daylight (Kiruna, Sweden)
  Geoposition position;
  position.latitude = 67.855800;
  position.longitude = 20.225282;
  position.status = Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = ToUTCTime(kNow);

  // Test that after sending the new position, sunrise and sunset time are
  // updated correctly.
  SetServerPosition(position);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(controller()->GetSunsetTime(),
            GeolocationController::kNoSunRiseSet);
  EXPECT_EQ(controller()->GetSunriseTime(),
            GeolocationController::kNoSunRiseSet);

  // 24 hours of darkness (Belgrano II Base, Antarctica)
  position.latitude = -77.87361f;
  position.longitude = -34.62745;

  // Test that after sending the new position, sunrise and sunset time are
  // updated correctly.
  SetServerPosition(position);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(controller()->GetSunsetTime(),
            GeolocationController::kNoSunRiseSet);
  EXPECT_EQ(controller()->GetSunriseTime(),
            GeolocationController::kNoSunRiseSet);
}

// Tests that if device sleeps more than a day, the geoposition is fetched
// instantly.
TEST_F(GeolocationControllerTest, RequestGeopositionAfterSuspend) {
  const base::TimeDelta zero_duration = base::Seconds(0);
  auto* power_manager_client = chromeos::FakePowerManagerClient::Get();
  const base::TimeDelta next_request_delay_after_success = base::Days(1);
  // Add an observer. Adding the first observer automatically requests a
  // geoposition instantly.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);
  EXPECT_EQ(0, observer.position_received_num());
  EXPECT_EQ(zero_duration, timer_ptr()->GetCurrentDelay());

  // Fetch that instant request to make the next request has a delay becomes
  // `kNextRequestDelayAfterSuccess`, i.e. `next_request_delay_after_success`,
  FireTimerToFetchGeoposition();
  EXPECT_EQ(next_request_delay_after_success, timer_ptr()->GetCurrentDelay());
  EXPECT_EQ(1, observer.position_received_num());

  // Suspend the device for a day and wake the device.
  power_manager_client->SendSuspendImminent(
      power_manager::SuspendImminent::Reason::SuspendImminent_Reason_IDLE);
  power_manager_client->SendSuspendDone(base::Days(1));
  // Test that after waking up from 1-day suspension, the controller request a
  // new geoposition instantly.
  EXPECT_EQ(zero_duration, timer_ptr()->GetCurrentDelay());
  FireTimerToFetchGeoposition();
  EXPECT_EQ(2, observer.position_received_num());
  EXPECT_EQ(next_request_delay_after_success, timer_ptr()->GetCurrentDelay());

  // Suspend the device for less than a day.
  power_manager_client->SendSuspendImminent(
      power_manager::SuspendImminent::Reason::SuspendImminent_Reason_IDLE);
  // Test that after waking up from 2-hr suspension, the controller continues
  // the old geoposition request with the same delay.
  power_manager_client->SendSuspendDone(base::Hours(2));
  EXPECT_EQ(next_request_delay_after_success, timer_ptr()->GetCurrentDelay());
}

// Tests the behavior when there is no valid geoposition for example due to lack
// of connectivity.
TEST_F(GeolocationControllerTest, AbsentValidGeoposition) {
  test_clock()->SetNow(ToUTCTime(kTestNow));

  // Initially, no values are stored in either of the two users' prefs.
  ASSERT_FALSE(user1_pref_service()->HasPrefPath(
      prefs::kDeviceGeolocationCachedLatitude));
  ASSERT_FALSE(user1_pref_service()->HasPrefPath(
      prefs::kDeviceGeolocationCachedLongitude));
  ASSERT_FALSE(user2_pref_service()->HasPrefPath(
      prefs::kDeviceGeolocationCachedLatitude));
  ASSERT_FALSE(user2_pref_service()->HasPrefPath(
      prefs::kDeviceGeolocationCachedLongitude));

  // Store fake geoposition in user 2's prefs.
  user2_pref_service()->SetDouble(prefs::kDeviceGeolocationCachedLatitude,
                                  kTestLatitude1);
  user2_pref_service()->SetDouble(prefs::kDeviceGeolocationCachedLongitude,
                                  kTestLongitude1);

  // Switch to user 2 and expect that geoposition is loaded from pref.
  SwitchActiveUser(kUser2Email);
  EXPECT_EQ(controller()->GetSunsetTime(), ToUTCTime(kTestSunsetTime1));
  EXPECT_EQ(controller()->GetSunriseTime(), ToUTCTime(kTestSunriseTime1));

  // Switching to user 1 should ignore the current geoposition since it's
  // a cached value from user 2's prefs rather than a newly-updated value.
  SwitchActiveUser(kUser1Email);
  EXPECT_EQ(controller()->GetSunsetTime(),
            TimeOfDay(kDefaultSunsetTimeOffsetMinutes)
                .SetClock(test_clock())
                .ToTimeToday());
  EXPECT_EQ(controller()->GetSunriseTime(),
            TimeOfDay(kDefaultSunriseTimeOffsetMinutes)
                .SetClock(test_clock())
                .ToTimeToday());

  // Now simulate receiving a live geoposition update.
  Geoposition position;
  position.latitude = kTestLatitude1;
  position.longitude = kTestLongitude1;
  position.status = Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = ToUTCTime(kTestNow);
  SetServerPosition(position);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(controller()->GetSunsetTime(), ToUTCTime(kTestSunsetTime1));
  EXPECT_EQ(controller()->GetSunriseTime(), ToUTCTime(kTestSunriseTime1));

  // Update user 2's prefs with different geoposition.
  user2_pref_service()->SetDouble(prefs::kDeviceGeolocationCachedLatitude,
                                  kTestLatitude2);
  user2_pref_service()->SetDouble(prefs::kDeviceGeolocationCachedLongitude,
                                  kTestLongitude2);

  // Now switching to user 2 should completely ignore their cached geopsoition,
  // since from now on we have a valid newly-retrieved value.
  SwitchActiveUser(kUser2Email);
  EXPECT_EQ(controller()->GetSunsetTime(), ToUTCTime(kTestSunsetTime1));
  EXPECT_EQ(controller()->GetSunriseTime(), ToUTCTime(kTestSunriseTime1));

  // Clear all cached geoposition prefs for all users, just to make sure getting
  // a new geoposition will persist it for all users not just the active one.
  user1_pref_service()->ClearPref(prefs::kDeviceGeolocationCachedLatitude);
  user1_pref_service()->ClearPref(prefs::kDeviceGeolocationCachedLongitude);
  user2_pref_service()->ClearPref(prefs::kDeviceGeolocationCachedLatitude);
  user2_pref_service()->ClearPref(prefs::kDeviceGeolocationCachedLongitude);

  // Now simulate receiving another live geoposition update.
  position.latitude = kTestLatitude2;
  position.longitude = kTestLongitude2;
  SetServerPosition(position);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(controller()->GetSunsetTime(), ToUTCTime(kTestSunsetTime2));
  EXPECT_EQ(controller()->GetSunriseTime(), ToUTCTime(kTestSunriseTime2));
  EXPECT_EQ(kTestLatitude2, user1_pref_service()->GetDouble(
                                prefs::kDeviceGeolocationCachedLatitude));
  EXPECT_EQ(kTestLongitude2, user1_pref_service()->GetDouble(
                                 prefs::kDeviceGeolocationCachedLongitude));
  EXPECT_EQ(kTestLatitude2, user2_pref_service()->GetDouble(
                                prefs::kDeviceGeolocationCachedLatitude));
  EXPECT_EQ(kTestLongitude2, user2_pref_service()->GetDouble(
                                 prefs::kDeviceGeolocationCachedLongitude));
}

}  // namespace

}  // namespace ash
