// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_on_demand_scheduler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestPrefName[] = "test_pref_name";

}  // namespace

class NearbyShareOnDemandSchedulerTest : public ::testing::Test {
 protected:
  NearbyShareOnDemandSchedulerTest() = default;
  ~NearbyShareOnDemandSchedulerTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kTestPrefName);
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    scheduler_ = std::make_unique<NearbyShareOnDemandScheduler>(
        /*retry_failures=*/true, /*require_connectivity=*/true, kTestPrefName,
        &pref_service_, base::DoNothing(), task_environment_.GetMockClock());
  }

  NearbyShareScheduler* scheduler() { return scheduler_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NearbyShareScheduler> scheduler_;
};

TEST_F(NearbyShareOnDemandSchedulerTest, NoRecurringRequest) {
  scheduler()->Start();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}
