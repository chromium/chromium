// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_suite.h"
#include "base/timer/timer.h"
#include "chrome/browser/metrics/perf/collection_params.h"
#include "chrome/browser/metrics/perf/metric_provider.h"
#include "chrome/browser/metrics/perf/perf_events_collector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);
const base::TimeDelta kMaxCollectionDelay = base::TimeDelta::FromSeconds(1);
// Use non-default 1-sec collection duration to make the test run faster.
const base::TimeDelta kCollectionDuration = base::TimeDelta::FromSeconds(1);
// The timeout in waiting until collection done. 8 sec is a safe value far
// beyond the collection duration used.
const base::TimeDelta kCollectionDoneTimeout = base::TimeDelta::FromSeconds(8);

class TestPerfCollector : public PerfCollector {
 public:
  explicit TestPerfCollector(const CollectionParams& params) : PerfCollector() {
    // Override default collection params.
    collection_params() = params;
  }
  ~TestPerfCollector() override = default;

  using internal::MetricCollector::collection_params;
};

class TestMetricProvider : public MetricProvider {
 public:
  using MetricProvider::MetricProvider;
  ~TestMetricProvider() override = default;

  using MetricProvider::set_cache_updated_callback;
};

// Allows access to some private methods for testing.
class TestProfileProvider : public ProfileProvider {
 public:
  TestProfileProvider() {
    CollectionParams test_params;
    test_params.collection_duration = kCollectionDuration;
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.resume_from_suspend.max_collection_delay = kMaxCollectionDelay;
    test_params.restore_session.sampling_factor = 1;
    test_params.restore_session.max_collection_delay = kMaxCollectionDelay;
    test_params.periodic_interval = kPeriodicCollectionInterval;

    collectors_.clear();
    auto metric_provider = std::make_unique<TestMetricProvider>(
        std::make_unique<TestPerfCollector>(test_params));
    metric_provider->set_cache_updated_callback(base::BindRepeating(
        &TestProfileProvider::OnProfileDone, base::Unretained(this)));

    collectors_.push_back(std::move(metric_provider));
  }

  void WaitUntilCollectionDone() {
    // Collection shouldn't be done when this method is called, or the test will
    // waste time in |run_loop_| for the duration of |timeout|.
    EXPECT_FALSE(collection_done());

    timeout_timer_.Start(FROM_HERE, kCollectionDoneTimeout,
                         base::BindLambdaForTesting([&]() {
                           // Collection is not done yet. Quit the run loop to
                           // fail the test.
                           run_loop_.Quit();
                         }));
    // The run loop returns when its Quit() is called either on collection done
    // or on timeout. Note that the second call of Quit() is a noop.
    run_loop_.Run();

    if (timeout_timer_.IsRunning())
      // Timer is still running: the run loop doesn't quit on timeout. Stop the
      // timer.
      timeout_timer_.Stop();
  }

  bool collection_done() { return collection_done_; }

  using ProfileProvider::collectors_;
  using ProfileProvider::OnJankStarted;
  using ProfileProvider::OnJankStopped;
  using ProfileProvider::OnSessionRestoreDone;
  using ProfileProvider::SuspendDone;

 private:
  void OnProfileDone() {
    collection_done_ = true;
    // Notify that profile collection is done. Quitting the run loop is
    // thread-safe.
    run_loop_.Quit();
  }

  base::OneShotTimer timeout_timer_;
  base::RunLoop run_loop_;
  bool collection_done_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestProfileProvider);
};

// This test doesn't mock any class used indirectly by ProfileProvider to make
// real collections from debugd.
class ProfileProviderRealCollectionTest : public testing::Test {
 public:
  ProfileProviderRealCollectionTest() {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    // ProfileProvider requires chromeos::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();

    std::map<std::string, std::string> field_trial_params;
    // Only "cycles" event is supported.
    field_trial_params.insert(std::make_pair(
        "PerfCommand::default::0", "50 perf record -a -e cycles -c 1000003"));
    field_trial_params.insert(
        std::make_pair("PerfCommand::default::1",
                       "50 perf record -a -e cycles -g -c 4000037"));
    ASSERT_TRUE(variations::AssociateVariationParams(
        "ChromeOSWideProfilingCollection", "group_name", field_trial_params));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        "ChromeOSWideProfilingCollection", "group_name");
    ASSERT_TRUE(field_trial_.get());

    profile_provider_ = std::make_unique<TestProfileProvider>();
    profile_provider_->Init();

    // Set user state as logged in. This activates periodic collection, but
    // other triggers like SUSPEND_DONE take precedence.
    chromeos::LoginState::Get()->SetLoggedInState(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR);

    // Finishes Init() on the dedicated sequence.
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    profile_provider_.reset();
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    variations::testing::ClearAllVariationParams();
  }

 protected:
  // |task_environment_| must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<base::FieldTrial> field_trial_;

  std::unique_ptr<TestProfileProvider> profile_provider_;

  DISALLOW_COPY_AND_ASSIGN(ProfileProviderRealCollectionTest);
};

TEST_F(ProfileProviderRealCollectionTest, SuspendDone) {
  // Trigger a resume from suspend.
  profile_provider_->SuspendDone(base::TimeDelta::FromMinutes(10));

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  std::vector<SampledProfile> stored_profiles;
  ASSERT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  auto& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
  ASSERT_TRUE(profile.has_perf_data());
}

TEST_F(ProfileProviderRealCollectionTest, SessionRestoreDOne) {
  // Restored 10 tabs.
  profile_provider_->OnSessionRestoreDone(10);

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  std::vector<SampledProfile> stored_profiles;
  ASSERT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  auto& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
  ASSERT_TRUE(profile.has_perf_data());
}

TEST_F(ProfileProviderRealCollectionTest, OnJankStarted) {
  // Trigger a resume from suspend.
  profile_provider_->OnJankStarted();

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  std::vector<SampledProfile> stored_profiles;
  ASSERT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  auto& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::JANKY_TASK, profile.trigger_event());
  ASSERT_TRUE(profile.has_perf_data());
}

TEST_F(ProfileProviderRealCollectionTest, OnJankStopped) {
  // Trigger a resume from suspend.
  profile_provider_->OnJankStarted();

  // Call ProfileProvider::OnJankStopped() halfway through the collection
  // duration.
  base::OneShotTimer stop_timer;
  base::RunLoop run_loop;
  stop_timer.Start(FROM_HERE, kCollectionDuration / 2,
                   base::BindLambdaForTesting([&]() {
                     profile_provider_->OnJankStopped();
                     run_loop.Quit();
                   }));
  run_loop.Run();
  // |run_loop| quits only by |stop_timer| so the timer shouldn't be running.
  EXPECT_FALSE(stop_timer.IsRunning());

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  std::vector<SampledProfile> stored_profiles;
  ASSERT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

  auto& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::JANKY_TASK, profile.trigger_event());
  ASSERT_TRUE(profile.has_perf_data());
}

}  // namespace metrics

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::RunUnitTestsUsingBaseTestSuite(argc, argv);
}
