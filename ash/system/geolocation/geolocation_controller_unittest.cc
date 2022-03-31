// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller.h"

#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller_test_util.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

namespace {

// Constructs a TimeZone object from the given `timezone_id`.
std::unique_ptr<icu::TimeZone> CreateTimezone(const char* timezone_id) {
  return base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id, -1, US_INV)));
}

std::u16string GetTimezoneId(const icu::TimeZone& timezone) {
  return chromeos::system::TimezoneSettings::GetTimezoneID(timezone);
}

// Base test fixture.
class GeolocationControllerTest : public AshTestBase {
 public:
  GeolocationControllerTest() = default;
  GeolocationControllerTest(const GeolocationControllerTest&) = delete;
  GeolocationControllerTest& operator=(const GeolocationControllerTest&) =
      delete;

  ~GeolocationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = ash::Shell::Get()->geolocation_controller();

    test_clock_.SetNow(base::Time::Now());

    std::unique_ptr<base::MockOneShotTimer> mock_timer =
        std::make_unique<base::MockOneShotTimer>();
    mock_timer_ptr_ = mock_timer.get();
    controller_->SetTimerForTesting(std::move(mock_timer));
    controller_->SetClockForTesting(&test_clock_);
    factory_ = static_cast<TestGeolocationUrlLoaderFactory*>(
        controller_->GetFactoryForTesting());

    // Prepare a valid geoposition.
    Geoposition position;
    position.latitude = 32.0;
    position.longitude = 31.0;
    position.status = Geoposition::STATUS_OK;
    position.accuracy = 10;
    position.timestamp = base::Time::Now();
    SetServerPosition(position);
  }

  GeolocationController* controller() const { return controller_; }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  base::MockOneShotTimer* mock_timer_ptr() const { return mock_timer_ptr_; }
  const Geoposition& position() const { return position_; }

  // Fires the timer of the scheduler to request geoposition and wait for all
  // observers to receive the latest geoposition from the server.
  void FireTimerToFetchGeoposition() {
    GeopositionResponsesWaiter waiter;
    // Make sure that the timer is running indicating that the client runs
    // the scheduler.
    EXPECT_TRUE(mock_timer_ptr()->IsRunning());
    // Fast forward the scheduler to reach the time when the controller
    // requests for geoposition from the server in
    // `GeolocationController::RequestGeoposition`.
    mock_timer_ptr_->Fire();
    // Waits for the observers to receive the geoposition from the server.
    waiter.Wait();
  }

  // Sets the geoposition to be returned from the `factory_` upon the
  // `GeolocationController` request.
  void SetServerPosition(const Geoposition& position) {
    position_ = position;
    factory_->set_position(position_);
  }

 private:
  GeolocationController* controller_;
  base::SimpleTestClock test_clock_;
  base::MockOneShotTimer* mock_timer_ptr_;
  TestGeolocationUrlLoaderFactory* factory_;
  Geoposition position_;
};

// Tests adding and removing an observer should request and stop receiving
// a position update.
TEST_F(GeolocationControllerTest, Observer) {
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());

  // Add an observer should start the timer requesting the geoposition.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer.position_received_num());

  // Check that the timer fires another schedule after a successful request.
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());

  // Removing an observer should stop the timer.
  controller()->RemoveObserver(&observer);
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());
  EXPECT_EQ(1, observer.position_received_num());
}

// Tests adding and removing observer and make sure that only observing ones
// receive the position updates.
TEST_F(GeolocationControllerTest, MultipleObservers) {
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());

  // Add an observer should start the timer requesting for the first
  // geoposition request.
  GeolocationControllerObserver observer1;
  controller()->AddObserver(&observer1);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer1.position_received_num());
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());

  // Since `OnGeoposition()` handling a geoposition update always schedule
  // the next geoposition request, the timer should keep running and
  // update position periodically.
  FireTimerToFetchGeoposition();
  EXPECT_EQ(2, observer1.position_received_num());
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());

  // Adding `observer2` should not interrupt the request flow. Check that both
  // observers receive the new position.
  GeolocationControllerObserver observer2;
  controller()->AddObserver(&observer2);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(1, observer2.position_received_num());
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());

  // Remove `observer1` and make sure that the timer is still running.
  // Only `observer2` should receive the new position.
  controller()->RemoveObserver(&observer1);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(2, observer2.position_received_num());
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());

  // Removing `observer2` should stop the timer. The request count should
  // not change.
  controller()->RemoveObserver(&observer2);
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());
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
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());
  controller()->AddObserver(&observer);

  // If the position is invalid, the controller won't push the geoposition
  // update to its observers.
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());
  mock_timer_ptr()->Fire();
  // Wait for the request and response to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer.position_received_num());
  // With error response, the server will retry with another timer which we
  // have no control over, so `mock_timer_ptr_` will not be running (refers to
  // `SimpleGeolocationRequest::Retry()` for more detail).
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());
}

// Tests that timezone changes result.
TEST_F(GeolocationControllerTest, TimezoneChanges) {
  EXPECT_FALSE(mock_timer_ptr()->IsRunning());
  controller()->SetCurrentTimezoneIdForTesting(u"America/Los_Angeles");

  // Add an observer.
  GeolocationControllerObserver observer;
  controller()->AddObserver(&observer);
  EXPECT_EQ(0, observer.position_received_num());
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer.position_received_num());
  EXPECT_EQ(u"America/Los_Angeles", controller()->current_timezone_id());
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());

  // A new timezone results in new geoposition request.
  auto timezone = CreateTimezone("Asia/Tokyo");

  controller()->TimezoneChanged(*timezone);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(2, observer.position_received_num());
  EXPECT_EQ(GetTimezoneId(*timezone), controller()->current_timezone_id());
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());
}

// Tests obtaining sunset/sunrise time when there is no valid geoposition, for
// example, due to lack of connectivity.
TEST_F(GeolocationControllerTest, SunsetSunriseDefault) {
  // Default sunset time at 6:00 PM as an offset from 00:00.
  constexpr int kDefaultSunsetTimeOffsetMinutes = 18 * 60;
  // Default sunrise time at 6:00 AM as an offset from 00:00.
  constexpr int kDefaultSunriseTimeOffsetMinutes = 6 * 60;

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
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString("23 Dec 2021 12:00:00", &now));
  test_clock()->SetNow(now);

  base::Time sunrise;
  EXPECT_TRUE(base::Time::FromUTCString("23 Dec 2021 04:14:36.626", &sunrise));
  base::Time sunset;
  EXPECT_TRUE(base::Time::FromUTCString("23 Dec 2021 14:59:58.459", &sunset));

  // Add an observer and make sure that sunset and sunrise time are not
  // updated until the timer is fired.
  GeolocationControllerObserver observer1;
  controller()->AddObserver(&observer1);
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());
  EXPECT_NE(controller()->GetSunsetTime(), sunset);
  EXPECT_NE(controller()->GetSunriseTime(), sunrise);
  EXPECT_EQ(0, observer1.position_received_num());

  // Prepare a valid geoposition.
  Geoposition position;
  position.latitude = 23.5;
  position.longitude = 35.88;
  position.status = Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = now;

  // Test that after sending the new position, sunrise and sunset time are
  // updated correctly.
  SetServerPosition(position);
  FireTimerToFetchGeoposition();
  EXPECT_EQ(1, observer1.position_received_num());
  EXPECT_EQ(controller()->GetSunsetTime(), sunset);
  EXPECT_EQ(controller()->GetSunriseTime(), sunrise);
  EXPECT_TRUE(mock_timer_ptr()->IsRunning());
}

}  // namespace

}  // namespace ash
