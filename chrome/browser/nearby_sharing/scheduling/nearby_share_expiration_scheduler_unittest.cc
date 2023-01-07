// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kTestPrefName[] = "test_pref_name";

constexpr base::TimeDelta kTestInitialNow = base::Days(100);
constexpr base::TimeDelta kTestExpirationTimeFromInitalNow = base::Minutes(123);

}  // namespace

class NearbyShareExpirationSchedulerTest : public ::testing::Test {
 protected:
  NearbyShareExpirationSchedulerTest() = default;
  ~NearbyShareExpirationSchedulerTest() override = default;

  void SetUp() override {
    FastForward(kTestInitialNow);
    expiration_time_ = Now() + kTestExpirationTimeFromInitalNow;

    pref_service_.registry()->RegisterDictionaryPref(kTestPrefName);
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    scheduler_ = std::make_unique<NearbyShareExpirationScheduler>(
        base::BindRepeating(
            &NearbyShareExpirationSchedulerTest::TestExpirationTimeFunctor,
            base::Unretained(this)),
        /*retry_failures=*/true, /*require_connectivity=*/true, kTestPrefName,
        &pref_service_, base::DoNothing(), task_environment_.GetMockClock());
  }

  absl::optional<base::Time> TestExpirationTimeFunctor() {
    return expiration_time_;
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  absl::optional<base::Time> expiration_time_;
  NearbyShareScheduler* scheduler() { return scheduler_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NearbyShareScheduler> scheduler_;
};

TEST_F(NearbyShareExpirationSchedulerTest, ExpirationRequest) {
  scheduler()->Start();

  // Let 5 minutes elapse since the start time just to make sure the time to the
  // next request only depends on the expiration time and the current time.
  FastForward(base::Minutes(5));

  EXPECT_EQ(*expiration_time_ - Now(), scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbyShareExpirationSchedulerTest, Reschedule) {
  scheduler()->Start();
  FastForward(base::Minutes(5));

  base::TimeDelta initial_expected_time_until_next_request =
      *expiration_time_ - Now();
  EXPECT_EQ(initial_expected_time_until_next_request,
            scheduler()->GetTimeUntilNextRequest());

  // The expiration time suddenly changes.
  expiration_time_ = *expiration_time_ + base::Days(2);
  scheduler()->Reschedule();
  EXPECT_EQ(initial_expected_time_until_next_request + base::Days(2),
            scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbyShareExpirationSchedulerTest, NullExpirationTime) {
  expiration_time_.reset();
  scheduler()->Start();
  EXPECT_EQ(absl::nullopt, scheduler()->GetTimeUntilNextRequest());
}
