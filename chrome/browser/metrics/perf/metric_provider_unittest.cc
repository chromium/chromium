// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_provider.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Returns an example PerfDataProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto() {
  PerfDataProto proto;
  proto.set_timestamp_sec(1435604013);  // Time since epoch in seconds.

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

  // MMapEvent.
  PerfDataProto_PerfEvent* event1 = proto.add_events();
  event1->mutable_header()->set_type(1);
  event1->mutable_mmap_event()->set_pid(1234);
  event1->mutable_mmap_event()->set_filename_md5_prefix(0xabcdefab);

  // CommEvent.
  PerfDataProto_PerfEvent* event2 = proto.add_events();
  event2->mutable_header()->set_type(2);
  event2->mutable_comm_event()->set_pid(5678);
  event2->mutable_comm_event()->set_comm_md5_prefix(0x0123456abcd);

  return proto;
}

// Converts a protobuf to serialized format as a byte vector.
std::vector<uint8_t> SerializeMessageToVector(
    const google::protobuf::MessageLite& message) {
  std::vector<uint8_t> result(message.ByteSize());
  message.SerializeToArray(result.data(), result.size());
  return result;
}

using TestSyncService = syncer::TestSyncService;

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestSyncService>();
}

// Allow access to some private methods for testing.
class TestMetricProvider : public MetricProvider {
 public:
  using MetricProvider::MetricProvider;
  ~TestMetricProvider() override = default;

  using MetricProvider::RecordAttemptStatus;
};

// Allows access to some private methods for testing.
class TestMetricCollector : public internal::MetricCollector {
 public:
  TestMetricCollector() : TestMetricCollector(CollectionParams()) {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : internal::MetricCollector("UMA.CWP.TestData", collection_params),
        weak_factory_(this) {}

  TestMetricCollector(const TestMetricCollector&) = delete;
  TestMetricCollector& operator=(const TestMetricCollector&) = delete;

  const char* ToolName() const override { return "Test"; }
  base::WeakPtr<internal::MetricCollector> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) override {
    PerfDataProto perf_data_proto = GetExamplePerfDataProto();
    SaveSerializedPerfProto(std::move(sampled_profile),
                            perf_data_proto.SerializeAsString());
  }

 private:
  base::WeakPtrFactory<TestMetricCollector> weak_factory_;
};

const base::TimeDelta kPeriodicCollectionInterval = base::Hours(1);
const base::TimeDelta kMaxCollectionDelay = base::Seconds(1);
const uint64_t kRedactedCommMd5Prefix = 0xee1f021828a1fcbc;
}  // namespace

class MetricProviderTest : public testing::Test {
 public:
  MetricProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MetricProviderTest(const MetricProviderTest&) = delete;
  MetricProviderTest& operator=(const MetricProviderTest&) = delete;

  void SetUp() override {
    CollectionParams test_params;
    // Set the sampling factors for the triggers to 1, so we always trigger
    // collection, and set the collection delays to well known quantities, so
    // we can fast forward the time.
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.resume_from_suspend.max_collection_delay = kMaxCollectionDelay;
    test_params.restore_session.sampling_factor = 1;
    test_params.restore_session.max_collection_delay = kMaxCollectionDelay;
    test_params.periodic_interval = kPeriodicCollectionInterval;

    metric_provider_ = std::make_unique<TestMetricProvider>(
        std::make_unique<TestMetricCollector>(test_params), nullptr);
    metric_provider_->Init();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override { metric_provider_.reset(); }

 protected:
  // task_environment_ must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestMetricProvider> metric_provider_;
};

TEST_F(MetricProviderTest, CheckSetup) {
  // There are no cached profiles at start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(MetricProviderTest, DisabledBeforeLogin) {
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  // There are no cached profiles after a profiling interval.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(MetricProviderTest, EnabledOnLogin) {
  metric_provider_->OnUserLoggedIn();
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  // We should find a cached PERIODIC_COLLECTION profile after a profiling
  // interval.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_FALSE(profile.has_suspend_duration_ms());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());
}

TEST_F(MetricProviderTest, DisabledOnDeactivate) {
  metric_provider_->OnUserLoggedIn();
  task_environment_.RunUntilIdle();

  metric_provider_->Deactivate();
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  // There are no cached profiles after a profiling interval.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(MetricProviderTest, SuspendDone) {
  metric_provider_->OnUserLoggedIn();
  task_environment_.RunUntilIdle();

  const auto kSuspendDuration = base::Minutes(3);

  metric_provider_->SuspendDone(kSuspendDuration);

  // Fast forward the time by the max collection delay.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // Check that the SuspendDone trigger produced one profile.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
  EXPECT_EQ(kSuspendDuration.InMilliseconds(), profile.suspend_duration_ms());
  EXPECT_TRUE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());
}

TEST_F(MetricProviderTest, OnSessionRestoreDone) {
  metric_provider_->OnUserLoggedIn();
  task_environment_.RunUntilIdle();

  const int kRestoredTabs = 7;

  metric_provider_->OnSessionRestoreDone(kRestoredTabs);

  // Fast forward the time by the max collection delay.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
  EXPECT_EQ(kRestoredTabs, profile.num_tabs_restored());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());
}

TEST_F(MetricProviderTest, ThermalStateRecordedInProfile) {
  metric_provider_->OnUserLoggedIn();
  metric_provider_->SetThermalState(
      base::PowerThermalObserver::DeviceThermalState::kSerious);
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  // We should find a cached PERIODIC_COLLECTION profile after a profiling
  // interval.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_EQ(THERMAL_STATE_SERIOUS, profile.thermal_state());
}

TEST_F(MetricProviderTest, DisableRecording) {
  base::HistogramTester histogram_tester;
  metric_provider_->OnUserLoggedIn();

  // Upon disabling recording, we would expect no cached profiles after a
  // profiling interval.
  metric_provider_->DisableRecording();
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.RecordTest",
      TestMetricProvider::RecordAttemptStatus::kRecordingDisabled, 1);
}

TEST_F(MetricProviderTest, EnableRecording) {
  base::HistogramTester histogram_tester;
  metric_provider_->OnUserLoggedIn();

  // Upon enabling recording, we would find a cached PERIODIC_COLLECTION profile
  // after a profiling interval.
  metric_provider_->EnableRecording();
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            stored_profiles[0].trigger_event());
  // When initializing the MetricProvider, we set the ProfileManager to nullptr.
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.RecordTest",
      TestMetricProvider::RecordAttemptStatus::kProfileManagerUnset, 1);
}

class MetricProviderSyncSettingsTest : public testing::Test {
 public:
  MetricProviderSyncSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MetricProviderSyncSettingsTest(const MetricProviderSyncSettingsTest&) =
      delete;
  MetricProviderSyncSettingsTest& operator=(
      const MetricProviderSyncSettingsTest&) = delete;

  void SetUp() override {
    CollectionParams test_params;
    test_params.periodic_interval = kPeriodicCollectionInterval;

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    // A Default profile is always loaded before all other user profiles are
    // initialized on Chrome OS. So creating the Default profile here to reflect
    // this. The Default profile is skipped when getting the sync settings from
    // user profile(s).
    testing_profile_manager_->CreateTestingProfile(
        ash::kSigninBrowserContextBaseName);
    // Also add two non-regular profiles that might appear on ChromeOS. They
    // always disable sync and are skipped when getting sync settings.
    testing_profile_manager_->CreateTestingProfile(
        ash::kLockScreenAppBrowserContextBaseName);
    testing_profile_manager_->CreateTestingProfile(
        ash::kLockScreenBrowserContextBaseName);
    metric_provider_ = std::make_unique<TestMetricProvider>(
        std::make_unique<TestMetricCollector>(test_params),
        testing_profile_manager_->profile_manager());
    metric_provider_->Init();
    task_environment_.RunUntilIdle();

    perf_data_unchanged_ = GetExamplePerfDataProto();
    // Perf data whose COMM event has its comm_md5_prefix redacted.
    perf_data_redacted_ = GetExamplePerfDataProto();
    perf_data_redacted_.mutable_events(1)
        ->mutable_comm_event()
        ->set_comm_md5_prefix(kRedactedCommMd5Prefix);
  }

  void TearDown() override {
    metric_provider_.reset();
    testing_profile_manager_.reset();
  }

 protected:
  TestSyncService* GetSyncService(TestingProfile* profile) {
    TestSyncService* sync_service = static_cast<TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, base::BindRepeating(&TestingSyncFactoryFunction)));
    sync_service->SetInitialSyncFeatureSetupComplete(true);
    return sync_service;
  }

  void EnableOSAppSync(TestSyncService* sync_service) {
    sync_service->GetUserSettings()->SetSelectedOsTypes(
        /*sync_all_os_types=*/false, {syncer::UserSelectableOsType::kOsApps});
  }

  void DisableOSAppSync(TestSyncService* sync_service) {
    sync_service->GetUserSettings()->SetSelectedOsTypes(
        /*sync_all_os_types=*/false, {});
  }

  void EnableAppSync(TestSyncService* sync_service) {
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kApps});
  }

  void DisableAppSync(TestSyncService* sync_service) {
    sync_service->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                      {});
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfileManager> testing_profile_manager_;

  std::unique_ptr<TestMetricProvider> metric_provider_;

  PerfDataProto perf_data_unchanged_;

  PerfDataProto perf_data_redacted_;
};

TEST_F(MetricProviderSyncSettingsTest, NoLoadedUserProfile) {
  // The Default profile is skipped and there is no other user profile
  // initialized. So we would expect the perf data to be redacted and a
  // histogram count of kNoLoadedProfile.
  base::HistogramTester histogram_tester;
  std::vector<SampledProfile> stored_profiles;
  metric_provider_->OnUserLoggedIn();

  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_EQ(SerializeMessageToVector(perf_data_redacted_),
            SerializeMessageToVector(profile.perf_data()));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.RecordTest",
      TestMetricProvider::RecordAttemptStatus::kNoLoadedProfile, 1);
}

TEST_F(MetricProviderSyncSettingsTest, SyncFeatureDisabled) {
  base::HistogramTester histogram_tester;
  std::vector<SampledProfile> stored_profiles;
  metric_provider_->OnUserLoggedIn();

  // The first testing profile has both sync-the-feature and App sync enabled.
  TestSyncService* sync_service1 =
      GetSyncService(testing_profile_manager_->CreateTestingProfile("user1"));
  EnableOSAppSync(sync_service1);

  // The second testing profile has kOsApps type enabled, but sync-the-feature
  // is turned off by marking FirstSetupComplete as false.
  TestSyncService* sync_service2 =
      GetSyncService(testing_profile_manager_->CreateTestingProfile("user2"));
  EnableOSAppSync(sync_service2);
  sync_service2->SetInitialSyncFeatureSetupComplete(false);

  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_EQ(SerializeMessageToVector(perf_data_redacted_),
            SerializeMessageToVector(profile.perf_data()));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.RecordTest",
      TestMetricProvider::RecordAttemptStatus::kChromeSyncFeatureDisabled, 1);
}

TEST_F(MetricProviderSyncSettingsTest, AppSyncEnabled) {
  base::HistogramTester histogram_tester;
  std::vector<SampledProfile> stored_profiles;
  metric_provider_->OnUserLoggedIn();

  // Set up two testing profiles, both with OS App Sync enabled. The Default
  // profile has OS App Sync disabled but is skipped.
  TestSyncService* sync_service1 =
      GetSyncService(testing_profile_manager_->CreateTestingProfile("user1"));
  TestSyncService* sync_service2 =
      GetSyncService(testing_profile_manager_->CreateTestingProfile("user2"));
  EnableOSAppSync(sync_service1);
  EnableOSAppSync(sync_service2);

  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_EQ(SerializeMessageToVector(perf_data_unchanged_),
            SerializeMessageToVector(profile.perf_data()));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.RecordTest",
      TestMetricProvider::RecordAttemptStatus::kAppSyncEnabled, 1);
}

TEST_F(MetricProviderSyncSettingsTest, AppSyncDisabled) {
  base::HistogramTester histogram_tester;
  std::vector<SampledProfile> stored_profiles;
  metric_provider_->OnUserLoggedIn();

  // Set up two testing profiles, one with OS App Sync enabled and the other
  // disabled.
  TestSyncService* sync_service1 =
      GetSyncService(testing_profile_manager_->CreateTestingProfile("user1"));
  TestSyncService* sync_service2 =
      GetSyncService(testing_profile_manager_->CreateTestingProfile("user2"));
  EnableOSAppSync(sync_service1);
  DisableOSAppSync(sync_service2);

  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  EXPECT_TRUE(metric_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_EQ(stored_profiles.size(), 1u);

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_EQ(SerializeMessageToVector(perf_data_redacted_),
            SerializeMessageToVector(profile.perf_data()));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.RecordTest",
      TestMetricProvider::RecordAttemptStatus::kOSAppSyncDisabled, 1);
}

}  // namespace metrics
