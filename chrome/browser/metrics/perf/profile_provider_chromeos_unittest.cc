// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/metric_provider.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Returns sample PerfDataProtos with custom timestamps. The contents don't have
// to make sense. They just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto(int tstamp_sec) {
  PerfDataProto proto;
  proto.set_timestamp_sec(tstamp_sec);  // Time since epoch in seconds.

  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  file_attr->add_ids(61);
  file_attr->add_ids(62);
  file_attr->add_ids(63);

  PerfDataProto_PerfEventAttr* attr = file_attr->mutable_attr();
  attr->set_type(1);
  attr->set_size(2);
  attr->set_config(3);
  attr->set_sample_period(4);
  attr->set_sample_freq(5);

  PerfDataProto_PerfEventStats* stats = proto.mutable_stats();
  stats->set_num_events_read(100);
  stats->set_num_sample_events(200);
  stats->set_num_mmap_events(300);
  stats->set_num_fork_events(400);
  stats->set_num_exit_events(500);

  return proto;
}

// Custome metric collectors to register with the profile provider for testing.
template <int TSTAMP>
class TestMetricCollector : public internal::MetricCollector {
 public:
  TestMetricCollector() : TestMetricCollector(CollectionParams()) {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : internal::MetricCollector("UMA.CWP.TestData", collection_params) {}

  TestMetricCollector(const TestMetricCollector&) = delete;
  TestMetricCollector& operator=(const TestMetricCollector&) = delete;

  const char* ToolName() const override { return "test"; }
  base::WeakPtr<internal::MetricCollector> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) override {
    PerfDataProto perf_data_proto = GetExamplePerfDataProto(TSTAMP);
    // Create an incognito observer to test initialization on the UI thread.
    auto observer = WindowedIncognitoMonitor::CreateObserver();
    if (!observer->IncognitoActive()) {
      SaveSerializedPerfProto(std::move(sampled_profile),
                              perf_data_proto.SerializeAsString());
    }
  }

 private:
  base::WeakPtrFactory<TestMetricCollector> weak_factory_{this};
};

const base::TimeDelta kPeriodicCollectionInterval = base::Hours(1);
const base::TimeDelta kMaxCollectionDelay = base::Seconds(1);

// Allows access to some private methods for testing.
class TestProfileProvider : public ProfileProvider {
 public:
  TestProfileProvider() {
    // Add a couple of metric collectors. We differentiate between them using
    // different time stamps for profiles. Set sampling factors for triggers
    // to 1, so we always trigger collection.
    CollectionParams test_params;
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.resume_from_suspend.max_collection_delay = kMaxCollectionDelay;
    test_params.restore_session.sampling_factor = 1;
    test_params.restore_session.max_collection_delay = kMaxCollectionDelay;
    test_params.periodic_interval = kPeriodicCollectionInterval;

    collectors_.clear();
    collectors_.push_back(std::make_unique<MetricProvider>(
        std::make_unique<TestMetricCollector<100>>(test_params), nullptr));
    collectors_.push_back(std::make_unique<MetricProvider>(
        std::make_unique<TestMetricCollector<200>>(test_params), nullptr));
  }

  using ProfileProvider::collectors_;
  using ProfileProvider::jank_monitor;
  using ProfileProvider::jankiness_collection_min_interval;
  using ProfileProvider::LoggedInStateChanged;
  using ProfileProvider::OnJankStarted;
  using ProfileProvider::OnSessionRestoreDone;
  using ProfileProvider::SuspendDone;

  TestProfileProvider(const TestProfileProvider&) = delete;
  TestProfileProvider& operator=(const TestProfileProvider&) = delete;
};

void ExpectTwoStoredPerfProfiles(
    const std::vector<SampledProfile>& stored_profiles,
    SampledProfile_TriggerEvent want_trigger_type,
    ThermalState want_thermal_state_type,
    int want_speed_limit) {
  ASSERT_EQ(2U, stored_profiles.size());
  // Both profiles must be of the given type and include perf data.
  const SampledProfile& profile1 = stored_profiles[0];
  const SampledProfile& profile2 = stored_profiles[1];
  EXPECT_EQ(want_trigger_type, profile1.trigger_event());
  ASSERT_TRUE(profile1.has_perf_data());
  EXPECT_EQ(want_trigger_type, profile2.trigger_event());
  ASSERT_TRUE(profile2.has_perf_data());
  // Both profiles must include the given thermal state,
  EXPECT_EQ(want_thermal_state_type, profile1.thermal_state());
  EXPECT_EQ(want_thermal_state_type, profile2.thermal_state());
  // ... and CPU speed limit.
  EXPECT_EQ(want_speed_limit, profile1.cpu_speed_limit_percent());
  EXPECT_EQ(want_speed_limit, profile2.cpu_speed_limit_percent());

  // We must have received a profile from each of the collectors.
  EXPECT_EQ(100u, profile1.perf_data().timestamp_sec());
  EXPECT_EQ(200u, profile2.perf_data().timestamp_sec());
}

}  // namespace

class ProfileProviderTest : public testing::Test {
 public:
  ProfileProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ProfileProviderTest(const ProfileProviderTest&) = delete;
  ProfileProviderTest& operator=(const ProfileProviderTest&) = delete;

  void SetUp() override {
    // ProfileProvider requires ash::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    ash::LoginState::Initialize();
    test_power_monitor_source_.GenerateThermalThrottlingEvent(
        base::PowerThermalObserver::DeviceThermalState::kNominal);

    profile_provider_ = std::make_unique<TestProfileProvider>();
    profile_provider_->Init();
  }

  void TearDown() override {
    profile_provider_.reset();
    ash::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  // task_environment_ must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedPowerMonitorTestSource test_power_monitor_source_;
  std::unique_ptr<TestProfileProvider> profile_provider_;
};

TEST_F(ProfileProviderTest, CheckSetup) {
  EXPECT_EQ(2U, profile_provider_->collectors_.size());

  // No profiles should be collected on start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, UserLoginLogout) {
  // No user is logged in, so no collection is scheduled to run.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());

  // Simulate a user log in, which should activate periodic collection for all
  // collectors.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);

  // Run all pending tasks. SetLoggedInState has activated timers for periodic
  // collection causing timer based pending tasks.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  // We should find two profiles, one for each collector.
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles(
      stored_profiles, SampledProfile::PERIODIC_COLLECTION,
      THERMAL_STATE_NOMINAL, base::PowerThermalObserver::kSpeedLimitMax);

  // Periodic collection is deactivated when user logs out. Simulate a user
  // logout event.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_NONE, ash::LoginState::LOGGED_IN_USER_NONE);
  // Run all pending tasks.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  // We should find no new profiles.
  stored_profiles.clear();
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, SuspendDone_NoUserLoggedIn_NoCollection) {
  // No user is logged in, so no collection is done on resume from suspend.
  profile_provider_->SuspendDone(base::Minutes(10));
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, CanceledSuspend_NoCollection) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Trigger a canceled suspend (zero sleep duration).
  profile_provider_->SuspendDone(base::Seconds(0));
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // We should find no profiles.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, SuspendDone) {
  // Set user state as logged in. This activates periodic collection, but other
  // triggers like SUSPEND_DONE take precedence.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);

  // Trigger a resume from suspend.
  profile_provider_->SuspendDone(base::Minutes(10));
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles(
      stored_profiles, SampledProfile::RESUME_FROM_SUSPEND,
      THERMAL_STATE_NOMINAL, base::PowerThermalObserver::kSpeedLimitMax);
}

TEST_F(ProfileProviderTest, OnSessionRestoreDone_NoUserLoggedIn_NoCollection) {
  // No user is logged in, so no collection is done on session restore.
  profile_provider_->OnSessionRestoreDone(nullptr, 10);
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, OnSessionRestoreDone) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Trigger a session restore.
  profile_provider_->OnSessionRestoreDone(nullptr, 10);
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles(stored_profiles, SampledProfile::RESTORE_SESSION,
                              THERMAL_STATE_NOMINAL,
                              base::PowerThermalObserver::kSpeedLimitMax);
}

TEST_F(ProfileProviderTest, ThermalStateChangesAreCaptured) {
  // Simulate a user log in, which should activate periodic collection for all
  // collectors.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);
  test_power_monitor_source_.GenerateThermalThrottlingEvent(
      base::PowerThermalObserver::DeviceThermalState::kCritical);

  // Run all pending tasks. SetLoggedInState has activated timers for periodic
  // collection causing timer based pending tasks.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles(
      stored_profiles, SampledProfile::PERIODIC_COLLECTION,
      THERMAL_STATE_CRITICAL, base::PowerThermalObserver::kSpeedLimitMax);
}

TEST_F(ProfileProviderTest, CpuSpeedChangesAreCaptured) {
  // Simulate a user log in, which should activate periodic collection for all
  // collectors.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);
  test_power_monitor_source_.GenerateSpeedLimitEvent(50);

  // Run all pending tasks. SetLoggedInState has activated timers for periodic
  // collection causing timer based pending tasks.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles(stored_profiles,
                              SampledProfile::PERIODIC_COLLECTION,
                              THERMAL_STATE_NOMINAL, 50);
}

// Test profile collection triggered when a jank starts.
TEST_F(ProfileProviderTest, JankMonitorCallbacks) {
  // Jankiness collection requires that the user is logged in.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);

  // Trigger a jankiness collection.
  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles(stored_profiles, SampledProfile::JANKY_TASK,
                              THERMAL_STATE_NOMINAL,
                              base::PowerThermalObserver::kSpeedLimitMax);
}

// Test throttling of JANKY_TASK collections: no consecutive collections within
// jankiness_collection_min_interval().
TEST_F(ProfileProviderTest, JankinessCollectionThrottled) {
  // Jankiness collection requires that the user is logged in.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);

  // The first JANKY_TASK collection should succeed.
  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  std::vector<SampledProfile> stored_profiles;

  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles(stored_profiles, SampledProfile::JANKY_TASK,
                              THERMAL_STATE_NOMINAL,
                              base::PowerThermalObserver::kSpeedLimitMax);

  stored_profiles.clear();

  // We are about to fast forward the clock a lot. Deactivate all collectors
  // to disable periodic collections.
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Fast forward time to 1 second before the throttling duration is over.
  task_environment_.FastForwardBy(
      profile_provider_->jankiness_collection_min_interval() -
      base::Seconds(1));

  // This collection within the minimum interval should be throttled.
  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  stored_profiles.clear();

  // Move the clock forward past the throttling duration. The next JANKY_TASK
  // collection should succeed.
  task_environment_.FastForwardBy(base::Seconds(1));

  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles(stored_profiles, SampledProfile::JANKY_TASK,
                              THERMAL_STATE_NOMINAL,
                              base::PowerThermalObserver::kSpeedLimitMax);
}

// This class enables the jank monitor to test collections triggered by jank
// callbacks from the jank monitor.
class ProfileProviderJankinessTest : public ProfileProviderTest {
 public:
  void SetUp() override {
    ProfileProviderTest::SetUp();
    // Jankiness collection requires that the user is logged in.
    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_REGULAR);
    // Deactivate each collectors to disable periodic collections.
    for (auto& collector : profile_provider_->collectors_) {
      collector->Deactivate();
    }
  }
};

// Test profile collection triggered by a UI thread jank.
TEST_F(ProfileProviderJankinessTest, JankMonitor_UI) {
  EXPECT_TRUE(profile_provider_->jank_monitor());
  // Post a janky task to the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // This is a janky task that runs for 2 seconds.
        task_environment_.FastForwardBy(base::Seconds(2));
      }));
  task_environment_.RunUntilIdle();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles(stored_profiles, SampledProfile::JANKY_TASK,
                              THERMAL_STATE_NOMINAL,
                              base::PowerThermalObserver::kSpeedLimitMax);
}

// Test profile collection triggered by an IO thread jank.
TEST_F(ProfileProviderJankinessTest, JankMonitor_IO) {
  EXPECT_TRUE(profile_provider_->jank_monitor());
  // Post a janky task to the IO thread.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // This is a janky task that runs for 2 seconds.
        task_environment_.FastForwardBy(base::Seconds(2));
      }));
  task_environment_.RunUntilIdle();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles(stored_profiles, SampledProfile::JANKY_TASK,
                              THERMAL_STATE_NOMINAL,
                              base::PowerThermalObserver::kSpeedLimitMax);
}

namespace {

class TestStockProfileProvider : public ProfileProvider {
 public:
  TestStockProfileProvider() = default;

  using ProfileProvider::collectors_;

  TestStockProfileProvider(const TestStockProfileProvider&) = delete;
  TestStockProfileProvider& operator=(const TestStockProfileProvider&) = delete;
};

}  // namespace

class ProfileProviderStockTest : public testing::Test {
 public:
  ProfileProviderStockTest() = default;

  ProfileProviderStockTest(const ProfileProviderStockTest&) = delete;
  ProfileProviderStockTest& operator=(const ProfileProviderStockTest&) = delete;

  void SetUp() override {
    // ProfileProvider requires ash::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    ash::LoginState::Initialize();
  }

  void TearDown() override {
    ash::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ProfileProviderStockTest, CheckSetup) {
  TestStockProfileProvider profile_provider;
  // We should have one collector registered.
  EXPECT_EQ(1u, profile_provider.collectors_.size());

  // After initialization, we should still have a single collector.
  profile_provider.Init();
  EXPECT_EQ(1u, profile_provider.collectors_.size());
}

}  // namespace metrics
