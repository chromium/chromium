// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/perf/heap_collector.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/metric_provider.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
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

  DISALLOW_COPY_AND_ASSIGN(TestMetricCollector);
};

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);
const base::TimeDelta kMaxCollectionDelay = base::TimeDelta::FromSeconds(1);

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
        std::make_unique<TestMetricCollector<100>>(test_params)));
    collectors_.push_back(std::make_unique<MetricProvider>(
        std::make_unique<TestMetricCollector<200>>(test_params)));
  }

  using ProfileProvider::collectors_;
  using ProfileProvider::jank_monitor;
  using ProfileProvider::jankiness_collection_min_interval;
  using ProfileProvider::LoggedInStateChanged;
  using ProfileProvider::OnJankStarted;
  using ProfileProvider::OnSessionRestoreDone;
  using ProfileProvider::SuspendDone;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProfileProvider);
};

template <SampledProfile_TriggerEvent TRIGGER_TYPE>
void ExpectTwoStoredPerfProfiles(
    const std::vector<SampledProfile>& stored_profiles) {
  ASSERT_EQ(2U, stored_profiles.size());
  // Both profiles must be of the given type and include perf data.
  const SampledProfile& profile1 = stored_profiles[0];
  const SampledProfile& profile2 = stored_profiles[1];
  EXPECT_EQ(TRIGGER_TYPE, profile1.trigger_event());
  ASSERT_TRUE(profile1.has_perf_data());
  EXPECT_EQ(TRIGGER_TYPE, profile2.trigger_event());
  ASSERT_TRUE(profile2.has_perf_data());

  // We must have received a profile from each of the collectors.
  EXPECT_EQ(100u, profile1.perf_data().timestamp_sec());
  EXPECT_EQ(200u, profile2.perf_data().timestamp_sec());
}

}  // namespace

class ProfileProviderTest : public testing::Test {
 public:
  ProfileProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    // ProfileProvider requires chromeos::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();

    profile_provider_ = std::make_unique<TestProfileProvider>();
    profile_provider_->Init();
  }

  void TearDown() override {
    profile_provider_.reset();
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  // task_environment_ must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestProfileProvider> profile_provider_;

  DISALLOW_COPY_AND_ASSIGN(ProfileProviderTest);
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
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);

  // Run all pending tasks. SetLoggedInState has activated timers for periodic
  // collection causing timer based pending tasks.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  // We should find two profiles, one for each collector.
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles<SampledProfile::PERIODIC_COLLECTION>(
      stored_profiles);

  // Periodic collection is deactivated when user logs out. Simulate a user
  // logout event.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_NONE,
      chromeos::LoginState::LOGGED_IN_USER_NONE);
  // Run all pending tasks.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  // We should find no new profiles.
  stored_profiles.clear();
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, SuspendDone_NoUserLoggedIn_NoCollection) {
  // No user is logged in, so no collection is done on resume from suspend.
  profile_provider_->SuspendDone(base::TimeDelta::FromMinutes(10));
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, CanceledSuspend_NoCollection) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Trigger a canceled suspend (zero sleep duration).
  profile_provider_->SuspendDone(base::TimeDelta::FromSeconds(0));
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
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);

  // Trigger a resume from suspend.
  profile_provider_->SuspendDone(base::TimeDelta::FromMinutes(10));
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles<SampledProfile::RESUME_FROM_SUSPEND>(
      stored_profiles);
}

TEST_F(ProfileProviderTest, OnSessionRestoreDone_NoUserLoggedIn_NoCollection) {
  // No user is logged in, so no collection is done on session restore.
  profile_provider_->OnSessionRestoreDone(10);
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, OnSessionRestoreDone) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Trigger a session restore.
  profile_provider_->OnSessionRestoreDone(10);
  // Run all pending tasks.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles<SampledProfile::RESTORE_SESSION>(stored_profiles);
}

// Test profile collection triggered when a jank starts.
TEST_F(ProfileProviderTest, JankMonitorCallbacks) {
  // Jankiness collection requires that the user is logged in.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);

  // Trigger a jankiness collection.
  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles<SampledProfile::JANKY_TASK>(stored_profiles);
}

// Test throttling of JANKY_TASK collections: no consecutive collections within
// jankiness_collection_min_interval().
TEST_F(ProfileProviderTest, JankinessCollectionThrottled) {
  // Jankiness collection requires that the user is logged in.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);

  // The first JANKY_TASK collection should succeed.
  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  std::vector<SampledProfile> stored_profiles;

  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles<SampledProfile::JANKY_TASK>(stored_profiles);

  stored_profiles.clear();

  // We are about to fast forward the clock a lot. Deactivate all collectors
  // to disable periodic collections.
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Fast forward time to 1 second before the throttling duration is over.
  task_environment_.FastForwardBy(
      profile_provider_->jankiness_collection_min_interval() -
      base::TimeDelta::FromSeconds(1));

  // This collection within the minimum interval should be throttled.
  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  stored_profiles.clear();

  // Move the clock forward past the throttling duration. The next JANKY_TASK
  // collection should succeed.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  profile_provider_->OnJankStarted();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles<SampledProfile::JANKY_TASK>(stored_profiles);
}

// This class enables the jank monitor to test collections triggered by jank
// callbacks from the jank monitor.
class ProfileProviderJankinessTest : public ProfileProviderTest {
 public:
  ProfileProviderJankinessTest() : ProfileProviderTest() {
    const base::Feature kBrowserJankinessProfiling{
        "BrowserJankinessProfiling", base::FEATURE_DISABLED_BY_DEFAULT};
    scoped_feature_list_.InitAndEnableFeature(kBrowserJankinessProfiling);
  }

  void SetUp() override {
    ProfileProviderTest::SetUp();
    // Jankiness collection requires that the user is logged in.
    chromeos::LoginState::Get()->SetLoggedInState(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR);
    // Deactivate each collectors to disable periodic collections.
    for (auto& collector : profile_provider_->collectors_) {
      collector->Deactivate();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test profile collection triggered by a UI thread jank.
TEST_F(ProfileProviderJankinessTest, JankMonitor_UI) {
  EXPECT_TRUE(profile_provider_->jank_monitor());
  // Post a janky task to the UI thread.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindLambdaForTesting([&]() {
        // This is a janky task that runs for 2 seconds.
        task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
      }));
  task_environment_.RunUntilIdle();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles<SampledProfile::JANKY_TASK>(stored_profiles);
}

// Test profile collection triggered by an IO thread jank.
TEST_F(ProfileProviderJankinessTest, JankMonitor_IO) {
  EXPECT_TRUE(profile_provider_->jank_monitor());
  // Post a janky task to the IO thread.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindLambdaForTesting([&]() {
        // This is a janky task that runs for 2 seconds.
        task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
      }));
  task_environment_.RunUntilIdle();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  EXPECT_EQ(2U, stored_profiles.size());
  ExpectTwoStoredPerfProfiles<SampledProfile::JANKY_TASK>(stored_profiles);
}

TEST(ProfileProviderJankinessParamTest, SetFeatureParam) {
  content::BrowserTaskEnvironment task_environment;

  // Enable the jankiness profiler feature.
  const base::Feature kBrowserJankinessProfiling{
      "BrowserJankinessProfiling", base::FEATURE_DISABLED_BY_DEFAULT};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBrowserJankinessProfiling);

  chromeos::PowerManagerClient::InitializeFake();
  chromeos::LoginState::Initialize();

  std::unique_ptr<TestProfileProvider> profile_provider =
      std::make_unique<TestProfileProvider>();
  profile_provider->Init();

  // Get default minimum interval is expected to be 30 minutes.
  EXPECT_EQ(profile_provider->jankiness_collection_min_interval(),
            base::TimeDelta::FromMinutes(30));

  profile_provider.reset();

  scoped_feature_list.Reset();

  // Init the feature with non-default feature param value.
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("JankinessCollectionMinIntervalSec", "180"));
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kBrowserJankinessProfiling, params);

  // Init an instance of TestProfileProvider and check that the feature param
  // value takes effect.
  profile_provider = std::make_unique<TestProfileProvider>();
  profile_provider->Init();
  EXPECT_EQ(profile_provider->jankiness_collection_min_interval(),
            base::TimeDelta::FromSeconds(180));

  profile_provider.reset();

  chromeos::LoginState::Shutdown();
  chromeos::PowerManagerClient::Shutdown();
}

namespace {

class TestParamsProfileProvider : public ProfileProvider {
 public:
  TestParamsProfileProvider() = default;

  using ProfileProvider::collectors_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestParamsProfileProvider);
};

}  // namespace

class ProfileProviderFeatureParamsTest : public testing::Test {
 public:
  ProfileProviderFeatureParamsTest() = default;

  void SetUp() override {
    // ProfileProvider requires chromeos::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();
  }

  void TearDown() override {
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileProviderFeatureParamsTest);
};

TEST_F(ProfileProviderFeatureParamsTest, HeapCollectorDisabled) {
  std::map<std::string, std::string> params;
  params.insert(
      std::make_pair(heap_profiling::kOOPHeapProfilingFeatureMode, "non-cwp"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  TestParamsProfileProvider profile_provider;
  // We should have one collector registered.
  EXPECT_EQ(1u, profile_provider.collectors_.size());

  // After initialization, we should still have a single collector, because the
  // sampling factor param is set to 0.
  profile_provider.Init();
  EXPECT_EQ(1u, profile_provider.collectors_.size());

  // Before destroying ScopedFeatureList, we need to finish SetUp() of each
  // registered collector, which accesses field trial params on its own
  // dedicated sequence.
  task_environment_.RunUntilIdle();
}

TEST_F(ProfileProviderFeatureParamsTest, HeapCollectorEnabled) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair(heap_profiling::kOOPHeapProfilingFeatureMode,
                               "cwp-tcmalloc"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  TestParamsProfileProvider profile_provider;
  // We should have one collector registered.
  EXPECT_EQ(1u, profile_provider.collectors_.size());

  // After initialization, if the new tcmalloc is enabled, we should have two
  // collectors, because the sampling factor param is set to 1. Otherwise, we
  // must still have one collector only.
  profile_provider.Init();
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  EXPECT_EQ(2u, profile_provider.collectors_.size());
#else
  EXPECT_EQ(1u, profile_provider.collectors_.size());
#endif

  // Before destroying ScopedFeatureList, we need to finish SetUp() of each
  // registered collector, which accesses field trial params on its own
  // dedicated sequence.
  task_environment_.RunUntilIdle();
}

}  // namespace metrics
