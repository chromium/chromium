// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_collector.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite.h"

namespace metrics {

namespace internal {

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

// Creates a serialized data stream containing a string with a field tag number.
std::string SerializeStringFieldWithTag(int field, const std::string& value) {
  std::string result;
  google::protobuf::io::StringOutputStream string_stream(&result);
  google::protobuf::io::CodedOutputStream output(&string_stream);

  using google::protobuf::internal::WireFormatLite;
  WireFormatLite::WriteTag(field, WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                           &output);
  output.WriteVarint32(value.size());
  output.WriteString(value);

  return result;
}

// Allows access to some private methods for testing.
class TestMetricCollector : public MetricCollector {
 public:
  TestMetricCollector() : TestMetricCollector(CollectionParams()) {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : MetricCollector("UMA.CWP.TestData", collection_params) {}

  const char* ToolName() const override { return "test"; }
  base::WeakPtr<MetricCollector> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) override {
    PerfDataProto perf_data_proto = GetExamplePerfDataProto();
    SaveSerializedPerfProto(std::move(sampled_profile),
                            perf_data_proto.SerializeAsString());
  }

  using MetricCollector::collection_params;
  using MetricCollector::CurrentTimerDelay;
  using MetricCollector::Init;
  using MetricCollector::IsRunning;
  using MetricCollector::login_time;
  using MetricCollector::RecordUserLogin;
  using MetricCollector::SaveSerializedPerfProto;
  using MetricCollector::ScheduleIntervalCollection;
  using MetricCollector::ScheduleSessionRestoreCollection;
  using MetricCollector::ScheduleSuspendDoneCollection;
  using MetricCollector::set_profile_done_callback;
  using MetricCollector::StopTimer;

 private:
  base::WeakPtrFactory<TestMetricCollector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestMetricCollector);
};

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);
const base::TimeDelta kMaxCollectionDelay = base::TimeDelta::FromSeconds(1);

}  // namespace

class MetricCollectorTest : public testing::Test {
 public:
  MetricCollectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        perf_data_proto_(GetExamplePerfDataProto()) {}

  void SaveProfile(std::unique_ptr<SampledProfile> sampled_profile) {
    cached_profile_data_.resize(cached_profile_data_.size() + 1);
    cached_profile_data_.back().Swap(sampled_profile.get());
  }

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

    metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
    metric_collector_->set_profile_done_callback(base::BindRepeating(
        &MetricCollectorTest::SaveProfile, base::Unretained(this)));
    metric_collector_->Init();
    // MetricCollector requires the user to be logged in.
    metric_collector_->RecordUserLogin(base::TimeTicks::Now());
  }

  void TearDown() override {
    metric_collector_.reset();
    cached_profile_data_.clear();
  }

 protected:
  // task_environment_ must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  std::vector<SampledProfile> cached_profile_data_;

  std::unique_ptr<TestMetricCollector> metric_collector_;

  // Store sample perf data protobuf for testing.
  PerfDataProto perf_data_proto_;

  DISALLOW_COPY_AND_ASSIGN(MetricCollectorTest);
};

TEST_F(MetricCollectorTest, CheckSetup) {
  EXPECT_GT(perf_data_proto_.ByteSize(), 0);

  // Timer is active after user logs in.
  EXPECT_TRUE(metric_collector_->IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());
}

TEST_F(MetricCollectorTest, EmptyProtosAreNotSaved) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(std::move(sampled_profile),
                                             std::string());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(cached_profile_data_.empty());
}

TEST_F(MetricCollectorTest, PerfDataProto) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile), perf_data_proto_.SerializeAsString());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_boot());
  EXPECT_TRUE(profile.has_ms_after_login());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_EQ(perf_data_proto_.SerializeAsString(),
            profile.perf_data().SerializeAsString());
}

TEST_F(MetricCollectorTest, PerfDataProto_UnknownFieldsDiscarded) {
  // First add some unknown fields to MMapEvent, CommEvent, PerfBuildID, and
  // StringAndMd5sumPrefix. The known field values don't have to make sense for
  // perf data. They are just padding to avoid having an otherwise empty proto.
  // The unknown field string contents don't have to make sense as serialized
  // data as the test is to discard them.

  // MMapEvent
  PerfDataProto_PerfEvent* event1 = perf_data_proto_.add_events();
  event1->mutable_header()->set_type(1);
  event1->mutable_mmap_event()->set_pid(1234);
  event1->mutable_mmap_event()->set_filename_md5_prefix(0xdeadbeef);
  // Missing field |MMapEvent::filename| has tag=6.
  *event1->mutable_mmap_event()->mutable_unknown_fields() =
      SerializeStringFieldWithTag(6, "/opt/google/chrome/chrome");

  // CommEvent
  PerfDataProto_PerfEvent* event2 = perf_data_proto_.add_events();
  event2->mutable_header()->set_type(2);
  event2->mutable_comm_event()->set_pid(5678);
  event2->mutable_comm_event()->set_comm_md5_prefix(0x900df00d);
  // Missing field |CommEvent::comm| has tag=3.
  *event2->mutable_comm_event()->mutable_unknown_fields() =
      SerializeStringFieldWithTag(3, "chrome");

  // PerfBuildID
  PerfDataProto_PerfBuildID* build_id = perf_data_proto_.add_build_ids();
  build_id->set_misc(3);
  build_id->set_pid(1337);
  build_id->set_filename_md5_prefix(0x9876543210);
  // Missing field |PerfBuildID::filename| has tag=4.
  *build_id->mutable_unknown_fields() =
      SerializeStringFieldWithTag(4, "/opt/google/chrome/chrome");

  // StringAndMd5sumPrefix
  PerfDataProto_StringMetadata* metadata =
      perf_data_proto_.mutable_string_metadata();
  metadata->mutable_perf_command_line_whole()->set_value_md5_prefix(
      0x123456789);
  // Missing field |StringAndMd5sumPrefix::value| has tag=1.
  *metadata->mutable_perf_command_line_whole()->mutable_unknown_fields() =
      SerializeStringFieldWithTag(1, "perf record -a -- sleep 1");

  // Serialize to string and make sure it can be deserialized.
  std::string perf_data_string = perf_data_proto_.SerializeAsString();
  PerfDataProto temp_proto;
  EXPECT_TRUE(temp_proto.ParseFromString(perf_data_string));

  // Now pass it to |metric_collector_|.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  // Perf data protos are saved from the collector task runner.
  metric_collector_->SaveSerializedPerfProto(std::move(sampled_profile),
                                             perf_data_string);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_perf_data());

  // The serialized form should be different because the unknown fields have
  // have been removed.
  EXPECT_NE(perf_data_string, profile.perf_data().SerializeAsString());

  // Check contents of stored protobuf.
  const PerfDataProto& stored_proto = profile.perf_data();
  ASSERT_EQ(2, stored_proto.events_size());

  // MMapEvent
  const PerfDataProto_PerfEvent& stored_event1 = stored_proto.events(0);
  EXPECT_EQ(1U, stored_event1.header().type());
  EXPECT_EQ(1234U, stored_event1.mmap_event().pid());
  EXPECT_EQ(0xdeadbeef, stored_event1.mmap_event().filename_md5_prefix());
  EXPECT_EQ(0U, stored_event1.mmap_event().unknown_fields().size());

  // CommEvent
  const PerfDataProto_PerfEvent& stored_event2 = stored_proto.events(1);
  EXPECT_EQ(2U, stored_event2.header().type());
  EXPECT_EQ(5678U, stored_event2.comm_event().pid());
  EXPECT_EQ(0x900df00d, stored_event2.comm_event().comm_md5_prefix());
  EXPECT_EQ(0U, stored_event2.comm_event().unknown_fields().size());

  // PerfBuildID
  ASSERT_EQ(1, stored_proto.build_ids_size());
  const PerfDataProto_PerfBuildID& stored_build_id = stored_proto.build_ids(0);
  EXPECT_EQ(3U, stored_build_id.misc());
  EXPECT_EQ(1337U, stored_build_id.pid());
  EXPECT_EQ(0x9876543210U, stored_build_id.filename_md5_prefix());
  EXPECT_EQ(0U, stored_build_id.unknown_fields().size());

  // StringAndMd5sumPrefix
  const PerfDataProto_StringMetadata& stored_metadata =
      stored_proto.string_metadata();
  EXPECT_EQ(0x123456789U,
            stored_metadata.perf_command_line_whole().value_md5_prefix());
  EXPECT_EQ(0U,
            stored_metadata.perf_command_line_whole().unknown_fields().size());
}

// Change |sampled_profile| between calls to SaveSerializedPerfProto().
TEST_F(MetricCollectorTest, MultipleCalls) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  // Perf data protos are saved from the collector task runner.
  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile), perf_data_proto_.SerializeAsString());
  task_environment_.RunUntilIdle();

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESTORE_SESSION);
  sampled_profile->set_ms_after_restore(3000);
  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile), perf_data_proto_.SerializeAsString());
  task_environment_.RunUntilIdle();

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  sampled_profile->set_suspend_duration_ms(60000);
  sampled_profile->set_ms_after_resume(1500);
  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile), perf_data_proto_.SerializeAsString());
  task_environment_.RunUntilIdle();

  ASSERT_EQ(3U, cached_profile_data_.size());

  {
    const SampledProfile& profile = cached_profile_data_[0];
    EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    ASSERT_TRUE(profile.has_perf_data());
    EXPECT_EQ(perf_data_proto_.SerializeAsString(),
              profile.perf_data().SerializeAsString());
  }

  {
    const SampledProfile& profile = cached_profile_data_[1];
    EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    EXPECT_EQ(3000, profile.ms_after_restore());
    ASSERT_TRUE(profile.has_perf_data());
    EXPECT_EQ(perf_data_proto_.SerializeAsString(),
              profile.perf_data().SerializeAsString());
  }

  {
    const SampledProfile& profile = cached_profile_data_[2];
    EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    EXPECT_EQ(60000, profile.suspend_duration_ms());
    EXPECT_EQ(1500, profile.ms_after_resume());
    ASSERT_TRUE(profile.has_perf_data());
    EXPECT_EQ(perf_data_proto_.SerializeAsString(),
              profile.perf_data().SerializeAsString());
  }
}

TEST_F(MetricCollectorTest, StopTimer) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->CollectProfile(std::move(sampled_profile));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metric_collector_->IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());

  // Timer is stopped by StopTimer(), but login time and cached profiles stay.
  metric_collector_->StopTimer();
  EXPECT_FALSE(metric_collector_->IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());

  EXPECT_FALSE(cached_profile_data_.empty());
}

TEST_F(MetricCollectorTest, ScheduleSuspendDoneCollection) {
  const auto kSuspendDuration = base::TimeDelta::FromMinutes(3);

  metric_collector_->ScheduleSuspendDoneCollection(kSuspendDuration);

  // The timer should be running at this point.
  EXPECT_TRUE(metric_collector_->IsRunning());

  // Fast forward the time by the max collection delay.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  // Check that the SuspendDone trigger produced one profile.
  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
  EXPECT_EQ(kSuspendDuration.InMilliseconds(), profile.suspend_duration_ms());
  EXPECT_TRUE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  // A profile collection rearms the timer for a new perodic collection.
  // Check that the timer is running.
  EXPECT_TRUE(metric_collector_->IsRunning());
  cached_profile_data_.clear();

  // Currently, any collection from another trigger event pushes the periodic
  // collection interval forward by kPeriodicCollectionInterval. Since we had
  // a SuspendDone collection, we should not see any new profiles during the
  // next periodic collection interval, but see one in the following interval.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval -
                                  kMaxCollectionDelay);
  EXPECT_TRUE(cached_profile_data_.empty());

  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  ASSERT_EQ(1U, cached_profile_data_.size());
  const SampledProfile& profile2 = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile2.trigger_event());
}

TEST_F(MetricCollectorTest, ScheduleSessionRestoreCollection) {
  const int kRestoredTabs = 7;

  metric_collector_->ScheduleSessionRestoreCollection(kRestoredTabs);

  // The timer should be running at this point.
  EXPECT_TRUE(metric_collector_->IsRunning());

  // Fast forward the time by the max collection delay.
  task_environment_.FastForwardBy(kMaxCollectionDelay);

  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
  EXPECT_EQ(kRestoredTabs, profile.num_tabs_restored());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  // Timer is rearmed for periodic collection after each collection.
  // Check that the timer is running.
  EXPECT_TRUE(metric_collector_->IsRunning());
  cached_profile_data_.clear();

  // A second SessionRestoreDone call is throttled.
  metric_collector_->ScheduleSessionRestoreCollection(1);

  // Fast forward the time by the max collection delay.
  task_environment_.FastForwardBy(kMaxCollectionDelay);
  // This should find no new session restore profiles.
  EXPECT_TRUE(cached_profile_data_.empty());

  // Currently, any collection from another trigger event pushes the periodic
  // collection interval forward by kPeriodicCollectionInterval. Since we had
  // a SessionRestore collection, we should not see any new profiles during the
  // current periodic collection interval, but see one in the next interval.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval -
                                  kMaxCollectionDelay * 2);
  EXPECT_TRUE(cached_profile_data_.empty());

  // Advance clock another collection interval. We should find a profile.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  ASSERT_EQ(1U, cached_profile_data_.size());
  const SampledProfile& profile2 = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile2.trigger_event());

  // Advance the clock another periodic collection interval. This run should
  // include a new periodic collection, but no session restore.
  cached_profile_data_.clear();
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  ASSERT_EQ(1U, cached_profile_data_.size());
  const SampledProfile& profile3 = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile3.trigger_event());
}

TEST_F(MetricCollectorTest, ScheduleIntervalCollection) {
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(metric_collector_->IsRunning());

  // Advance the clock by a periodic collection interval. We must have a
  // periodic collection profile.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_FALSE(profile.has_suspend_duration_ms());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_EQ(perf_data_proto_.SerializeAsString(),
            profile.perf_data().SerializeAsString());

  // Make sure timer is rearmed after each collection.
  EXPECT_TRUE(metric_collector_->IsRunning());
}

// Setting the sampling factors to zero should disable the triggers.
// Otherwise, it could cause a div-by-zero crash.
TEST_F(MetricCollectorTest, ZeroSamplingFactorDisablesTrigger) {
  // Define params with zero sampling factors.
  CollectionParams test_params;
  test_params.resume_from_suspend.sampling_factor = 0;
  test_params.restore_session.sampling_factor = 0;

  metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
  metric_collector_->Init();
  metric_collector_->RecordUserLogin(base::TimeTicks::Now());

  // Cancel the background collection.
  metric_collector_->StopTimer();

  EXPECT_FALSE(metric_collector_->IsRunning())
      << "Sanity: timer should not be running.";

  // Calling ScheduleSuspendDoneCollection or ScheduleSessionRestoreCollection
  // should not start the timer that triggers collection.
  metric_collector_->ScheduleSuspendDoneCollection(
      base::TimeDelta::FromMinutes(10));
  EXPECT_FALSE(metric_collector_->IsRunning());

  metric_collector_->ScheduleSessionRestoreCollection(100);
  EXPECT_FALSE(metric_collector_->IsRunning());
}

TEST_F(MetricCollectorTest, ZeroPeriodicIntervalDisablesCollection) {
  // Define params with zero periodic interval.
  CollectionParams test_params;
  test_params.periodic_interval = base::TimeDelta::FromMilliseconds(0);

  metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
  metric_collector_->Init();
  metric_collector_->RecordUserLogin(base::TimeTicks::Now());

  EXPECT_FALSE(metric_collector_->IsRunning())
      << "Sanity: timer should not be running.";

  // Advance the clock by 10 hours. We should have no profile and timer is not
  // running.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(10));

  EXPECT_FALSE(metric_collector_->IsRunning())
      << "Sanity: timer should not be running.";

  ASSERT_TRUE(cached_profile_data_.empty());
}

}  // namespace internal

}  // namespace metrics
