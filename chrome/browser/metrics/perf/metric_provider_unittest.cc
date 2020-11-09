// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_provider.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind_test_util.h"
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

  return proto;
}

// Allows access to some private methods for testing.
class TestMetricCollector : public internal::MetricCollector {
 public:
  TestMetricCollector() : TestMetricCollector(CollectionParams()) {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : internal::MetricCollector("UMA.CWP.TestData", collection_params),
        weak_factory_(this) {}

  const char* ToolName() const override { return "test"; }
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

  DISALLOW_COPY_AND_ASSIGN(TestMetricCollector);
};

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);
const base::TimeDelta kMaxCollectionDelay = base::TimeDelta::FromSeconds(1);

}  // namespace

class MetricProviderTest : public testing::Test {
 public:
  MetricProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

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

    metric_provider_ = std::make_unique<MetricProvider>(
        std::make_unique<TestMetricCollector>(test_params));
    metric_provider_->Init();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override { metric_provider_.reset(); }

 protected:
  // task_environment_ must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MetricProvider> metric_provider_;

  DISALLOW_COPY_AND_ASSIGN(MetricProviderTest);
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

  const auto kSuspendDuration = base::TimeDelta::FromMinutes(3);

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

}  // namespace metrics
