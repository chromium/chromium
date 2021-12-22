// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller.h"

#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
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

// An observer class to GeolocationController which updates sunset and sunrise
// time.
class GeolocationControllerObserver : public GeolocationController::Observer {
 public:
  GeolocationControllerObserver() = default;

  GeolocationControllerObserver(const GeolocationControllerObserver&) = delete;
  GeolocationControllerObserver& operator=(
      const GeolocationControllerObserver&) = delete;

  ~GeolocationControllerObserver() override = default;

  // TODO(crbug.com/1269915): Add `sunset_` and `sunrise_` and update their
  // values when receiving the new position.
  void OnGeopositionChanged(bool possible_change_in_timezone) override {
    position_received_num_++;
  }

  SimpleGeoposition position() const { return position_; }
  int position_received_num() const { return position_received_num_; }

 private:
  SimpleGeoposition position_;

  // The number of times a new position is received.
  int position_received_num_ = 0;
};

// A fake implementation of GeolocationController that doesn't perform any
// actual geoposition requests.
class FakeGeolocationController : public GeolocationController {
 public:
  FakeGeolocationController(base::SimpleTestClock* test_clock,
                            std::unique_ptr<base::MockOneShotTimer> mock_timer)
      : GeolocationController(/*url_context_getter=*/nullptr) {
    SetTimerForTesting(std::move(mock_timer));
    SetClockForTesting(test_clock);
  }

  FakeGeolocationController(const FakeGeolocationController&) = delete;
  FakeGeolocationController& operator=(const FakeGeolocationController&) =
      delete;

  ~FakeGeolocationController() override = default;

  void set_position_to_send(const Geoposition& position) {
    position_to_send_ = position;
  }

  const Geoposition& position_to_send() const { return position_to_send_; }

  int geoposition_requests_num() const { return geoposition_requests_num_; }

 private:
  // GeolocationController:
  void RequestGeoposition() override {
    OnGeoposition(position_to_send_, /*server_error=*/false, base::TimeDelta());
    ++geoposition_requests_num_;
  }

  // The position to send to the controller the next time OnGeoposition is
  // invoked.
  Geoposition position_to_send_;

  // The number of new geoposition requests that have been triggered.
  int geoposition_requests_num_ = 0;
};

// Base test fixture.
class GeolocationControllerTest : public AshTestBase {
 public:
  GeolocationControllerTest() {
    test_clock_.SetNow(base::Time::Now());

    std::unique_ptr<base::MockOneShotTimer> mock_timer =
        std::make_unique<base::MockOneShotTimer>();
    mock_timer_ptr_ = mock_timer.get();
    controller_ = std::make_unique<FakeGeolocationController>(
        &test_clock_, std::move(mock_timer));

    // Prepare a valid geoposition.
    Geoposition position;
    position.latitude = 32.0;
    position.longitude = 31.0;
    position.status = Geoposition::STATUS_OK;
    position.accuracy = 10;
    position.timestamp = base::Time::Now();
    controller_->set_position_to_send(position);
  }

  GeolocationControllerTest(const GeolocationControllerTest&) = delete;
  GeolocationControllerTest& operator=(const GeolocationControllerTest&) =
      delete;

  ~GeolocationControllerTest() override = default;

 protected:
  std::unique_ptr<FakeGeolocationController> controller_;
  base::SimpleTestClock test_clock_;
  base::MockOneShotTimer* mock_timer_ptr_;
};

// Tests adding and removing observer and make sure that only observing ones
// receive the position updates.
TEST_F(GeolocationControllerTest, MultipleObservers) {
  EXPECT_EQ(0, controller_->geoposition_requests_num());
  EXPECT_FALSE(mock_timer_ptr_->IsRunning());

  // Add an observer should start the timer requesting for the first
  // geoposition request.
  GeolocationControllerObserver observer1;
  controller_->AddObserver(&observer1);
  EXPECT_TRUE(mock_timer_ptr_->IsRunning());
  mock_timer_ptr_->Fire();
  EXPECT_EQ(1, controller_->geoposition_requests_num());
  EXPECT_EQ(1, observer1.position_received_num());

  // Since `OnGeoposition()` handling a geoposition update always schedule
  // the next geoposition request, the timer should keep running and
  // update position periodically.
  EXPECT_TRUE(mock_timer_ptr_->IsRunning());
  mock_timer_ptr_->Fire();
  EXPECT_EQ(2, controller_->geoposition_requests_num());
  EXPECT_EQ(2, observer1.position_received_num());

  // Adding `observer2` should not interrupt the request flow. Check that both
  // observers receive the new position.
  GeolocationControllerObserver observer2;
  controller_->AddObserver(&observer2);
  mock_timer_ptr_->Fire();
  EXPECT_EQ(3, controller_->geoposition_requests_num());
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(1, observer2.position_received_num());

  // Remove `observer1` and make sure that the timer is still running.
  // Only `observer2` should receive the new position.
  controller_->RemoveObserver(&observer1);
  mock_timer_ptr_->Fire();
  EXPECT_EQ(4, controller_->geoposition_requests_num());
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(2, observer2.position_received_num());

  // Removing `observer2` should stop the timer. The request count should
  // not change.
  controller_->RemoveObserver(&observer2);
  EXPECT_FALSE(mock_timer_ptr_->IsRunning());
  EXPECT_EQ(4, controller_->geoposition_requests_num());
  EXPECT_EQ(3, observer1.position_received_num());
  EXPECT_EQ(2, observer2.position_received_num());
}

// Tests that controller only pushes valid positions.
TEST_F(GeolocationControllerTest, InvalidPositions) {
  EXPECT_EQ(0, controller_->geoposition_requests_num());
  GeolocationControllerObserver observer;
  // Update to an invalid position
  Geoposition position = controller_->position_to_send();
  position.status = Geoposition::STATUS_TIMEOUT;
  controller_->set_position_to_send(position);
  EXPECT_FALSE(mock_timer_ptr_->IsRunning());
  controller_->AddObserver(&observer);
  EXPECT_TRUE(mock_timer_ptr_->IsRunning());

  // If the position is invalid, the controller won't push the geoposition
  // update to its observers.
  mock_timer_ptr_->Fire();
  EXPECT_EQ(1, controller_->geoposition_requests_num());
  EXPECT_EQ(0, observer.position_received_num());

  // If the position is valid, the controller pushes the update to observers.
  position.status = Geoposition::STATUS_OK;
  controller_->set_position_to_send(position);
  mock_timer_ptr_->Fire();
  EXPECT_EQ(2, controller_->geoposition_requests_num());
  EXPECT_EQ(1, observer.position_received_num());
}

// Tests that timezone changes result.
TEST_F(GeolocationControllerTest, TimezoneChanges) {
  EXPECT_EQ(0, controller_->geoposition_requests_num());
  EXPECT_FALSE(mock_timer_ptr_->IsRunning());
  controller_->SetCurrentTimezoneIdForTesting(u"America/Los_Angeles");

  // Add an observer.
  GeolocationControllerObserver observer;
  controller_->AddObserver(&observer);
  EXPECT_EQ(0, observer.position_received_num());
  EXPECT_TRUE(mock_timer_ptr_->IsRunning());
  mock_timer_ptr_->Fire();
  EXPECT_EQ(1, controller_->geoposition_requests_num());
  EXPECT_EQ(1, observer.position_received_num());
  EXPECT_EQ(u"America/Los_Angeles", controller_->current_timezone_id());

  // A new timezone results in new geoposition request.
  auto timezone = CreateTimezone("Asia/Tokyo");

  controller_->TimezoneChanged(*timezone);
  mock_timer_ptr_->Fire();
  EXPECT_EQ(2, controller_->geoposition_requests_num());
  EXPECT_EQ(2, observer.position_received_num());
  EXPECT_EQ(GetTimezoneId(*timezone), controller_->current_timezone_id());
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
  EXPECT_EQ(controller_->GetSunsetTime(),
            TimeOfDay(kDefaultSunsetTimeOffsetMinutes).ToTimeToday());
  EXPECT_EQ(controller_->GetSunriseTime(),
            TimeOfDay(kDefaultSunriseTimeOffsetMinutes).ToTimeToday());
}

// TODO(crbug.com/1269915): Add a test for `GetSunsetTime()` and
// `GetSunriseTime()` along with updating `TimeOfDay` to support setting a
// clock to `test_clock_`, so the time can be tested deterministically.

}  // namespace

}  // namespace ash