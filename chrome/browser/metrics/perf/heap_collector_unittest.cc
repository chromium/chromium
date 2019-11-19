// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/heap_collector.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Returns a sample PerfDataProto for a heap profile.
PerfDataProto GetSampleHeapPerfDataProto() {
  PerfDataProto proto;

  // Add file attributes.
  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  PerfDataProto_PerfEventAttr* event_attr = file_attr->mutable_attr();
  event_attr->set_type(1);    // PERF_TYPE_SOFTWARE
  event_attr->set_size(96);   // PERF_ATTR_SIZE_VER3
  event_attr->set_config(9);  // PERF_COUNT_SW_DUMMY
  event_attr->set_sample_id_all(true);
  event_attr->set_sample_period(111222);
  event_attr->set_sample_type(1 /*PERF_SAMPLE_IP*/ | 2 /*PERF_SAMPLE_TID*/ |
                              32 /*PERF_SAMPLE_CALLCHAIN*/ |
                              64 /*PERF_SAMPLE_ID*/ |
                              256 /*PERF_SAMPLE_PERIOD*/);
  event_attr->set_mmap(true);
  file_attr->add_ids(0);

  PerfDataProto_PerfEventType* event_type = proto.add_event_types();
  event_type->set_id(9);  // PERF_COUNT_SW_DUMMY

  file_attr = proto.add_file_attrs();
  event_attr = file_attr->mutable_attr();
  event_attr->set_type(1);    // PERF_TYPE_SOFTWARE
  event_attr->set_size(96);   // PERF_ATTR_SIZE_VER3
  event_attr->set_config(9);  // PERF_COUNT_SW_DUMMY
  event_attr->set_sample_id_all(true);
  event_attr->set_sample_period(111222);
  event_attr->set_sample_type(1 /*PERF_SAMPLE_IP*/ | 2 /*PERF_SAMPLE_TID*/ |
                              32 /*PERF_SAMPLE_CALLCHAIN*/ |
                              64 /*PERF_SAMPLE_ID*/ |
                              256 /*PERF_SAMPLE_PERIOD*/);
  file_attr->add_ids(1);

  event_type = proto.add_event_types();
  event_type->set_id(9);  // PERF_COUNT_SW_DUMMY

  // Add MMAP event.
  PerfDataProto_PerfEvent* event = proto.add_events();
  PerfDataProto_EventHeader* header = event->mutable_header();
  header->set_type(1);  // PERF_RECORD_MMAP
  header->set_misc(0);
  header->set_size(0);

  PerfDataProto_MMapEvent* mmap = event->mutable_mmap_event();
  mmap->set_pid(3456);
  mmap->set_tid(3456);
  mmap->set_start(0x617aa770f000);
  mmap->set_len(0x617ab0689000 - 0x617aa770f000);
  mmap->set_pgoff(16);

  PerfDataProto_SampleInfo* sample_info = mmap->mutable_sample_info();
  sample_info->set_pid(3456);
  sample_info->set_tid(3456);
  sample_info->set_id(0);

  // Add Sample events.
  event = proto.add_events();
  header = event->mutable_header();
  header->set_type(9);  // PERF_RECORD_SAMPLE
  header->set_misc(2);  // PERF_RECORD_MISC_USER
  header->set_size(0);

  double scale = 1 / (1 - exp(-(1024.00 / 2.00) / 111222.00));

  PerfDataProto_SampleEvent* sample = event->mutable_sample_event();
  sample->set_ip(0x617aae951c31);
  sample->set_pid(3456);
  sample->set_tid(3456);
  sample->set_id(0);
  sample->set_period(2 * scale);
  sample->add_callchain(static_cast<uint64_t>(-512));  // PERF_CONTEXT_USER
  sample->add_callchain(0x617aae951c31);
  sample->add_callchain(0x617aae95062e);

  event = proto.add_events();
  header = event->mutable_header();
  header->set_type(9);  // PERF_RECORD_SAMPLE
  header->set_misc(2);  // PERF_RECORD_MISC_USER
  header->set_size(0);

  sample = event->mutable_sample_event();
  sample->set_ip(0x617aae951c31);
  sample->set_pid(3456);
  sample->set_tid(3456);
  sample->set_id(1);
  sample->set_period(1024 * scale);
  sample->add_callchain(static_cast<uint64_t>(-512));  // PERF_CONTEXT_USER
  sample->add_callchain(0x617aae951c31);
  sample->add_callchain(0x617aae95062e);

  return proto;
}

// Allows testing of HeapCollector behavior when an incognito window is opened.
class TestIncognitoObserver : public WindowedIncognitoObserver {
 public:
  // Factory function to create a TestIncognitoObserver object contained in a
  // std::unique_ptr<WindowedIncognitoObserver> object. |incognito_launched|
  // simulates the presence of an open incognito window, or the lack thereof.
  // Used for passing observers to ParseOutputProtoIfValid().
  static std::unique_ptr<WindowedIncognitoObserver> CreateWithIncognitoLaunched(
      bool incognito_launched) {
    return base::WrapUnique(new TestIncognitoObserver(incognito_launched));
  }

  bool IncognitoLaunched() const override { return incognito_launched_; }

 private:
  explicit TestIncognitoObserver(bool incognito_launched)
      : WindowedIncognitoObserver(nullptr, 0),
        incognito_launched_(incognito_launched) {}

  bool incognito_launched_;

  DISALLOW_COPY_AND_ASSIGN(TestIncognitoObserver);
};

// Allows access to some private methods for testing.
class TestHeapCollector : public HeapCollector {
 public:
  TestHeapCollector() : HeapCollector(HeapCollectionMode::kTcmalloc) {}
  explicit TestHeapCollector(HeapCollectionMode mode) : HeapCollector(mode) {}

  using HeapCollector::AddCachedDataDelta;
  using HeapCollector::collection_params;
  using HeapCollector::DumpProfileToTempFile;
  using HeapCollector::Init;
  using HeapCollector::IsEnabled;
  using HeapCollector::IsRunning;
  using HeapCollector::MakeQuipperCommand;
  using HeapCollector::Mode;
  using HeapCollector::ParseAndSaveProfile;
  using HeapCollector::RecordUserLogin;
  using HeapCollector::set_profile_done_callback;

  DISALLOW_COPY_AND_ASSIGN(TestHeapCollector);
};

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
size_t HeapSamplingPeriod(const TestHeapCollector& collector) {
  CHECK_EQ(collector.Mode(), HeapCollectionMode::kTcmalloc)
      << "Reading heap sampling period works only with tcmalloc sampling";
  size_t sampling_period;
  CHECK(base::allocator::GetNumericProperty("tcmalloc.sampling_period_bytes",
                                            &sampling_period))
      << "Failed to read heap sampling period";
  return sampling_period;
}
#endif

}  // namespace

class HeapCollectorTest : public testing::Test {
 public:
  HeapCollectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SaveProfile(std::unique_ptr<SampledProfile> sampled_profile) {
    cached_profile_data_.resize(cached_profile_data_.size() + 1);
    cached_profile_data_.back().Swap(sampled_profile.get());
  }

  void MakeHeapCollector(HeapCollectionMode mode) {
    heap_collector_ = std::make_unique<TestHeapCollector>(mode);
    // Set the periodic collection delay to a well known quantity, so we can
    // fast forward the time.
    heap_collector_->collection_params().periodic_interval =
        kPeriodicCollectionInterval;
    heap_collector_->set_profile_done_callback(base::BindRepeating(
        &HeapCollectorTest::SaveProfile, base::Unretained(this)));

    heap_collector_->Init();
    // HeapCollector requires the user to be logged in.
    heap_collector_->RecordUserLogin(base::TimeTicks::Now());
  }

  void TearDown() override {
    heap_collector_.reset();
    cached_profile_data_.clear();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::vector<SampledProfile> cached_profile_data_;

  std::unique_ptr<TestHeapCollector> heap_collector_;

  DISALLOW_COPY_AND_ASSIGN(HeapCollectorTest);
};

TEST_F(HeapCollectorTest, CheckTestIncognitoObserver) {
  EXPECT_FALSE(TestIncognitoObserver::CreateWithIncognitoLaunched(false)
                   ->IncognitoLaunched());
  EXPECT_TRUE(TestIncognitoObserver::CreateWithIncognitoLaunched(true)
                  ->IncognitoLaunched());
}

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
TEST_F(HeapCollectorTest, CheckSetup_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);

  // No profiles are cached on start.
  EXPECT_TRUE(cached_profile_data_.empty());

  // Heap sampling is enabled.
  size_t sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_GT(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, NoCollectionWhenProfileCacheFull_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(heap_collector_->IsRunning());
  // Pretend the cache is full.
  heap_collector_->AddCachedDataDelta(4 * 1024 * 1024);

  // Advance the clock by a periodic collection interval. We shouldn't find a
  // profile because the cache is full.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  EXPECT_TRUE(cached_profile_data_.empty());
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile_NoIncognito_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);

  auto incognito_observer =
      TestIncognitoObserver::CreateWithIncognitoLaunched(false);
  base::Optional<base::FilePath> got_path =
      heap_collector_->DumpProfileToTempFile(std::move(incognito_observer));
  // Check that we got a path.
  ASSERT_TRUE(got_path);
  // Check that the file is readable and not empty.
  base::File temp(got_path.value(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(temp.IsValid());
  EXPECT_GT(temp.GetLength(), 0);
  temp.Close();
  // We must be able to remove the temp file.
  ASSERT_TRUE(base::DeleteFile(got_path.value(), false));
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile_IncognitoOpened_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);

  auto incognito_observer =
      TestIncognitoObserver::CreateWithIncognitoLaunched(true);
  base::Optional<base::FilePath> got_path =
      heap_collector_->DumpProfileToTempFile(std::move(incognito_observer));
  // Check that we got a path.
  ASSERT_FALSE(got_path);
}

TEST_F(HeapCollectorTest, ParseAndSaveProfile_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);

  // Write a sample perf data proto to a temp file.
  const base::FilePath kTempProfile(
      FILE_PATH_LITERAL("/tmp/ParseAndSaveProfile.test"));
  PerfDataProto heap_proto = GetSampleHeapPerfDataProto();
  std::string serialized_proto = heap_proto.SerializeAsString();

  base::File temp(kTempProfile, base::File::FLAG_CREATE_ALWAYS |
                                    base::File::FLAG_READ |
                                    base::File::FLAG_WRITE);
  EXPECT_TRUE(temp.created());
  EXPECT_TRUE(temp.IsValid());
  int res = temp.WriteAtCurrentPos(serialized_proto.c_str(),
                                   serialized_proto.length());
  EXPECT_EQ(res, static_cast<int>(serialized_proto.length()));
  temp.Close();

  // Create a command line that copies the input file to the output.
  base::CommandLine::StringVector argv;
  argv.push_back("cat");
  argv.push_back(kTempProfile.value());
  auto cat = std::make_unique<base::CommandLine>(argv);

  // Run the command.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  heap_collector_->ParseAndSaveProfile(std::move(cat), kTempProfile,
                                       std::move(sampled_profile));
  task_environment_.RunUntilIdle();

  // Check that the profile was cached.
  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_boot());
  EXPECT_TRUE(profile.has_ms_after_login());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_EQ(serialized_proto, profile.perf_data().SerializeAsString());

  // Check that the temp profile file is removed after pending tasks complete.
  temp =
      base::File(kTempProfile, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_FALSE(temp.IsValid());
}
#endif

TEST_F(HeapCollectorTest, CheckSetup_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);

  // No profiles are cached on start.
  EXPECT_TRUE(cached_profile_data_.empty());

  // Heap sampling is enabled.
  EXPECT_TRUE(heap_collector_->IsEnabled());
}

TEST_F(HeapCollectorTest, NoCollectionWhenProfileCacheFull_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(heap_collector_->IsRunning());
  // Pretend the cache is full.
  heap_collector_->AddCachedDataDelta(4 * 1024 * 1024);

  // Advance the clock by a periodic collection interval. We shouldn't find a
  // profile because the cache is full.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);

  EXPECT_TRUE(cached_profile_data_.empty());
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile_NoIncognito_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);

  auto incognito_observer =
      TestIncognitoObserver::CreateWithIncognitoLaunched(false);
  base::Optional<base::FilePath> got_path =
      heap_collector_->DumpProfileToTempFile(std::move(incognito_observer));
  // Check that we got a path.
  ASSERT_TRUE(got_path);
  // Check that the file is readable and not empty.
  base::File temp(got_path.value(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(temp.IsValid());
  EXPECT_GT(temp.GetLength(), 0);
  temp.Close();
  // We must be able to remove the temp file.
  ASSERT_TRUE(base::DeleteFile(got_path.value(), false));
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile_IncognitoOpened_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);

  auto incognito_observer =
      TestIncognitoObserver::CreateWithIncognitoLaunched(true);
  base::Optional<base::FilePath> got_path =
      heap_collector_->DumpProfileToTempFile(std::move(incognito_observer));
  // Check that we got a path.
  ASSERT_FALSE(got_path);
}

TEST_F(HeapCollectorTest, MakeQuipperCommand) {
  const base::FilePath kTempProfile(
      FILE_PATH_LITERAL("/tmp/MakeQuipperCommand.test"));
  std::unique_ptr<base::CommandLine> got =
      TestHeapCollector::MakeQuipperCommand(kTempProfile);
  ASSERT_TRUE(got);

  // Check that we got the correct two switch names.
  ASSERT_EQ(got->GetSwitches().size(), 2u);
  EXPECT_TRUE(got->HasSwitch("input_heap_profile"));
  EXPECT_TRUE(got->HasSwitch("pid"));

  // Check that we got the correct program name and switch values.
  EXPECT_EQ(got->GetProgram().value(), "/usr/bin/quipper");
  EXPECT_EQ(got->GetSwitchValuePath("input_heap_profile"), kTempProfile);
  EXPECT_EQ(got->GetSwitchValueASCII("pid"),
            base::NumberToString(base::GetCurrentProcId()));
}

TEST_F(HeapCollectorTest, ParseAndSaveProfile_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);

  // Write a sample perf data proto to a temp file.
  const base::FilePath kTempProfile(
      FILE_PATH_LITERAL("/tmp/ParseAndSaveProfile.test"));
  PerfDataProto heap_proto = GetSampleHeapPerfDataProto();
  std::string serialized_proto = heap_proto.SerializeAsString();

  base::File temp(kTempProfile, base::File::FLAG_CREATE_ALWAYS |
                                    base::File::FLAG_READ |
                                    base::File::FLAG_WRITE);
  EXPECT_TRUE(temp.created());
  EXPECT_TRUE(temp.IsValid());
  int res = temp.WriteAtCurrentPos(serialized_proto.c_str(),
                                   serialized_proto.length());
  EXPECT_EQ(res, static_cast<int>(serialized_proto.length()));
  temp.Close();

  // Create a command line that copies the input file to the output.
  base::CommandLine::StringVector argv;
  argv.push_back("cat");
  argv.push_back(kTempProfile.value());
  auto cat = std::make_unique<base::CommandLine>(argv);

  // Run the command.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  heap_collector_->ParseAndSaveProfile(std::move(cat), kTempProfile,
                                       std::move(sampled_profile));
  task_environment_.RunUntilIdle();

  // Check that the profile was cached.
  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_boot());
  EXPECT_TRUE(profile.has_ms_after_login());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_EQ(serialized_proto, profile.perf_data().SerializeAsString());

  // Check that the temp profile file is removed after pending tasks complete.
  temp =
      base::File(kTempProfile, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_FALSE(temp.IsValid());
}

class HeapCollectorCollectionParamsTest : public testing::Test {
 public:
  HeapCollectorCollectionParamsTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(HeapCollectorCollectionParamsTest);
};

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
TEST_F(HeapCollectorCollectionParamsTest, ParametersOverride_Tcmalloc) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("SamplingIntervalBytes", "800000"));
  params.insert(std::make_pair("PeriodicCollectionIntervalMs", "3600000"));
  params.insert(std::make_pair("ResumeFromSuspend::SamplingFactor", "1"));
  params.insert(std::make_pair("ResumeFromSuspend::MaxDelaySec", "10"));
  params.insert(std::make_pair("RestoreSession::SamplingFactor", "2"));
  params.insert(std::make_pair("RestoreSession::MaxDelaySec", "20"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  auto heap_collector =
      std::make_unique<TestHeapCollector>(HeapCollectionMode::kTcmalloc);
  const auto& parsed_params = heap_collector->collection_params();

  // Not initialized yet:
  size_t sampling_period = HeapSamplingPeriod(*heap_collector);
  EXPECT_NE(800000u, sampling_period);
  EXPECT_NE(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);

  heap_collector->Init();

  sampling_period = HeapSamplingPeriod(*heap_collector);
  EXPECT_EQ(800000u, sampling_period);
  EXPECT_EQ(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);
}
#endif

TEST_F(HeapCollectorCollectionParamsTest, ParametersOverride_ShimLayer) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("SamplingIntervalBytes", "800000"));
  params.insert(std::make_pair("PeriodicCollectionIntervalMs", "3600000"));
  params.insert(std::make_pair("ResumeFromSuspend::SamplingFactor", "1"));
  params.insert(std::make_pair("ResumeFromSuspend::MaxDelaySec", "10"));
  params.insert(std::make_pair("RestoreSession::SamplingFactor", "2"));
  params.insert(std::make_pair("RestoreSession::MaxDelaySec", "20"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  auto heap_collector =
      std::make_unique<TestHeapCollector>(HeapCollectionMode::kShimLayer);
  const auto& parsed_params = heap_collector->collection_params();

  // Not initialized yet:
  EXPECT_NE(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);

  heap_collector->Init();

  EXPECT_EQ(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);
}

namespace {

std::vector<base::SamplingHeapProfiler::Sample> SamplingHeapProfilerSamples() {
  // Generate a sample using the SamplingHeapProfiler collector. Then, duplicate
  // and customize their values.
  base::SamplingHeapProfiler::Init();
  auto* collector = base::SamplingHeapProfiler::Get();
  collector->Start();

  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->SetSamplingInterval(1000000);
  // Generate and remove a dummy sample, because the first sample is potentially
  // ignored by the SHP profiler.
  sampler->RecordAlloc(reinterpret_cast<void*>(1), 1000000,
                       base::PoissonAllocationSampler::AllocatorType::kMalloc,
                       nullptr);
  sampler->RecordFree(reinterpret_cast<void*>(1));
  // Generate a second sample that we are going to retrieve.
  sampler->RecordAlloc(reinterpret_cast<void*>(2), 1000000,
                       base::PoissonAllocationSampler::AllocatorType::kMalloc,
                       nullptr);

  auto samples = collector->GetSamples(0);
  EXPECT_EQ(1lu, samples.size());
  sampler->RecordFree(reinterpret_cast<void*>(2));
  collector->Stop();

  // Customize the sample.
  auto& sample1 = samples[0];
  sample1.size = 100;
  sample1.total = 1000;
  sample1.stack = {reinterpret_cast<void*>(0x10000),
                   reinterpret_cast<void*>(0x10100),
                   reinterpret_cast<void*>(0x10200)};

  samples.emplace_back(samples.back());
  EXPECT_EQ(2lu, samples.size());
  auto& sample2 = samples[1];
  sample2.size = 200;
  sample2.total = 2000;
  sample2.stack = {
      reinterpret_cast<void*>(0x20000), reinterpret_cast<void*>(0x20100),
      reinterpret_cast<void*>(0x20200), reinterpret_cast<void*>(0x20300)};

  samples.emplace_back(samples.back());
  EXPECT_EQ(3lu, samples.size());
  auto& sample3 = samples[2];
  sample3.size = 300;
  sample3.total = 3000;
  sample3.stack = {reinterpret_cast<void*>(0x30000),
                   reinterpret_cast<void*>(0x30100),
                   reinterpret_cast<void*>(0x30200)};

  return samples;
}

std::string GetProcMaps() {
  return R"text(304acba71000-304acba72000 ---p 00000000 00:00 0
304acba72000-304acd86a000 rw-p 00000000 00:00 0
304acd86a000-304acd86b000 ---p 00000000 00:00 0
304acd86b000-304acd88a000 rw-p 00000000 00:00 0
304acd88a000-304acd88b000 ---p 00000000 00:00 0
304acd88b000-304acd8aa000 rw-p 00000000 00:00 0
5ffa92db8000-5ffa93d15000 r--p 00000000 b3:03 71780                      /opt/google/chrome/chrome
5ffa93d15000-5ffa93d16000 r--p 00f5d000 b3:03 71780                      /opt/google/chrome/chrome
5ffa93d16000-5ffa93d17000 r--p 00f5e000 b3:03 71780                      /opt/google/chrome/chrome
5ffa93d17000-5ffa9d176000 r-xp 00f5f000 b3:03 71780                      /opt/google/chrome/chrome
5ffa9d176000-5ffa9d1ca000 rw-p 0a3be000 b3:03 71780                      /opt/google/chrome/chrome
5ffa9d1ca000-5ffa9d8ff000 r--p 0a412000 b3:03 71780                      /opt/google/chrome/chrome
5ffa9d8ff000-5ffa9db6c000 rw-p 00000000 00:00 0
7f9c9e11c000-7f9c9e11d000 ---p 00000000 00:00 0
7f9c9e11d000-7f9c9e91d000 rw-p 00000000 00:00 0                          [stack:1843]
7f9c9e91d000-7f9c9e91e000 ---p 00000000 00:00 0
7f9c9e91e000-7f9c9f11e000 rw-p 00000000 00:00 0
7f9ca0d47000-7f9ca0d4c000 r-xp 00000000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0d4c000-7f9ca0f4b000 ---p 00005000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0f4b000-7f9ca0f4c000 r--p 00004000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0f4c000-7f9ca0f4d000 rw-p 00005000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0f4d000-7f9ca114d000 rw-s 00000000 00:13 16382                      /dev/shm/.com.google.Chrome.nohIdv (deleted)
7f9ca26a0000-7f9ca26a1000 ---p 00000000 00:00 0
7f9ca26a1000-7f9ca2ea1000 rw-p 00000000 00:00 0                          [stack:1796]
7f9ca2ea1000-7f9ca2f31000 r-xp 00000000 b3:03 46120                      /lib64/libpcre.so.1.2.9
7f9ca2f31000-7f9ca2f32000 r--p 0008f000 b3:03 46120                      /lib64/libpcre.so.1.2.9
7f9ca2f32000-7f9ca2f33000 rw-p 00090000 b3:03 46120                      /lib64/libpcre.so.1.2.9
7f9ca2f33000-7f9ca302d000 r-xp 00000000 b3:03 40207                      /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca302e000-7f9ca302f000 r--p 000fa000 b3:03 40207                      /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca4687000-7f9ca4887000 rw-s 00000000 00:13 12699                      /dev/shm/.com.google.Chrome.KWz8dN (deleted)
7f9ca4887000-7f9ca4888000 ---p 00000000 00:00 0
7f9ca4888000-7f9ca5088000 rw-p 00000000 00:00 0                          [stack:1423]
7f9ca52c8000-7f9ca52c9000 ---p 00000000 00:00 0
7f9ca52c9000-7f9ca5ac9000 rw-p 00000000 00:00 0                          [stack:1411]
7f9ca5aec000-7f9ca5b66000 r--p 00000000 b3:03 39083                      /usr/share/fonts/roboto/Roboto-Light.ttf
7f9ca5b66000-7f9ca5d66000 rw-s 00000000 00:13 10081                      /dev/shm/.com.google.Chrome.MLuvgs (deleted)
)text";
}

}  // namespace

TEST(HeapCollectorShimLayerTest, WriteHeapProfileToFile_InvalidProcMaps) {
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp(temp_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  ASSERT_TRUE(temp.created());
  ASSERT_TRUE(temp.IsValid());

  auto samples = SamplingHeapProfilerSamples();
  std::string proc_maps = "Bogus proc maps\n";
  EXPECT_FALSE(internal::WriteHeapProfileToFile(&temp, samples, proc_maps));
  temp.Close();
  base::DeleteFile(temp_path, false);
}

TEST(HeapCollectorShimLayerTest, WriteHeapProfileToFile) {
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp(temp_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  ASSERT_TRUE(temp.created());
  ASSERT_TRUE(temp.IsValid());

  auto samples = SamplingHeapProfilerSamples();
  auto proc_maps = GetProcMaps();
  EXPECT_TRUE(internal::WriteHeapProfileToFile(&temp, samples, proc_maps));
  temp.Close();

  std::string got;
  EXPECT_TRUE(base::ReadFileToString(temp_path, &got));

  std::string want = R"text(heap profile: 3: 6000 [3: 6000] @ heap_v2/1
1: 1000 [1: 1000] @ 0x10000 0x10100 0x10200
1: 2000 [1: 2000] @ 0x20000 0x20100 0x20200 0x20300
1: 3000 [1: 3000] @ 0x30000 0x30100 0x30200

MAPPED_LIBRARIES:
304acba71000-304acba72000 ---p 00000000 00:00 0 
304acba72000-304acd86a000 rw-p 00000000 00:00 0 
304acd86a000-304acd86b000 ---p 00000000 00:00 0 
304acd86b000-304acd88a000 rw-p 00000000 00:00 0 
304acd88a000-304acd88b000 ---p 00000000 00:00 0 
304acd88b000-304acd8aa000 rw-p 00000000 00:00 0 
5ffa92db8000-5ffa93d15000 r--p 00000000 00:00 0 /opt/google/chrome/chrome
5ffa93d15000-5ffa93d16000 r--p 00f5d000 00:00 0 /opt/google/chrome/chrome
5ffa93d16000-5ffa93d17000 r--p 00f5e000 00:00 0 /opt/google/chrome/chrome
5ffa93d17000-5ffa9d176000 r-xp 00f5f000 00:00 0 /opt/google/chrome/chrome
5ffa9d176000-5ffa9d1ca000 rw-p 0a3be000 00:00 0 /opt/google/chrome/chrome
5ffa9d1ca000-5ffa9d8ff000 r--p 0a412000 00:00 0 /opt/google/chrome/chrome
5ffa9d8ff000-5ffa9db6c000 rw-p 00000000 00:00 0 
7f9c9e11c000-7f9c9e11d000 ---p 00000000 00:00 0 
7f9c9e11d000-7f9c9e91d000 rw-p 00000000 00:00 0 [stack:1843]
7f9c9e91d000-7f9c9e91e000 ---p 00000000 00:00 0 
7f9c9e91e000-7f9c9f11e000 rw-p 00000000 00:00 0 
7f9ca0d47000-7f9ca0d4c000 r-xp 00000000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0d4c000-7f9ca0f4b000 ---p 00005000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0f4b000-7f9ca0f4c000 r--p 00004000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0f4c000-7f9ca0f4d000 rw-p 00005000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0f4d000-7f9ca114d000 rw-- 00000000 00:00 0 /dev/shm/.com.google.Chrome.nohIdv (deleted)
7f9ca26a0000-7f9ca26a1000 ---p 00000000 00:00 0 
7f9ca26a1000-7f9ca2ea1000 rw-p 00000000 00:00 0 [stack:1796]
7f9ca2ea1000-7f9ca2f31000 r-xp 00000000 00:00 0 /lib64/libpcre.so.1.2.9
7f9ca2f31000-7f9ca2f32000 r--p 0008f000 00:00 0 /lib64/libpcre.so.1.2.9
7f9ca2f32000-7f9ca2f33000 rw-p 00090000 00:00 0 /lib64/libpcre.so.1.2.9
7f9ca2f33000-7f9ca302d000 r-xp 00000000 00:00 0 /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca302e000-7f9ca302f000 r--p 000fa000 00:00 0 /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca4687000-7f9ca4887000 rw-- 00000000 00:00 0 /dev/shm/.com.google.Chrome.KWz8dN (deleted)
7f9ca4887000-7f9ca4888000 ---p 00000000 00:00 0 
7f9ca4888000-7f9ca5088000 rw-p 00000000 00:00 0 [stack:1423]
7f9ca52c8000-7f9ca52c9000 ---p 00000000 00:00 0 
7f9ca52c9000-7f9ca5ac9000 rw-p 00000000 00:00 0 [stack:1411]
7f9ca5aec000-7f9ca5b66000 r--p 00000000 00:00 0 /usr/share/fonts/roboto/Roboto-Light.ttf
7f9ca5b66000-7f9ca5d66000 rw-- 00000000 00:00 0 /dev/shm/.com.google.Chrome.MLuvgs (deleted)
)text";

  EXPECT_EQ(want, got);
  base::DeleteFile(temp_path, false);
}

}  // namespace metrics
