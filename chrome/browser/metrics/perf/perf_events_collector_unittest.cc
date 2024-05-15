// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_events_collector.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/perf/cpu_identity.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

const char kPerfCommandDelimiter[] = " ";

const char kPerfCyclesHGCmd[] = "-- record -a -e cycles:HG -c 1000003";
const char kPerfFPCallgraphHGCmd[] = "-- record -a -e cycles:HG -g -c 4000037";
const char kPerfLBRCallgraphCmd[] =
    "-- record -a -e cycles -c 6000011 --call-graph lbr";
const char kPerfCyclesPPPHGCmd[] = "-- record -a -e cycles:pppHG -c 1000003";
const char kPerfFPCallgraphPPPHGCmd[] =
    "-- record -a -e cycles:pppHG -g -c 4000037";
const char kPerfLBRCallgraphPPPCmd[] =
    "-- record -a -e cycles:ppp -c 6000011 --call-graph lbr";
const char kPerfLBRCmd[] = "-- record -a -e r20c4 -b -c 800011";
const char kPerfLBRCmdAtom[] = "-- record -a -e rc4 -b -c 800011";
const char kPerfLBRCmdTremont[] = "-- record -a -e rc0c4 -b -c 800011";
const char kPerfLBRCmdAlderLake[] =
    "-- record -a -e cpu_core/r20c4/ -e cpu_atom/rc0c4/ -b -c 800011";
const char kPerfITLBMissCyclesCmdIvyBridge[] =
    "-- record -a -e itlb_misses.walk_duration -c 30001";
const char kPerfITLBMissCyclesCmdSkylake[] =
    "-- record -a -e itlb_misses.walk_pending -c 30001";
const char kPerfITLBMissCyclesCmdAtom[] =
    "-- record -a -e page_walks.i_side_cycles -c 30001";
const char kPerfITLBMissCyclesCmdTremont[] = "-- record -a -e r1085 -c 30001";
const char kPerfITLBMissCyclesCmdAlderLake[] =
    "-- record -a -e cpu_core/r1011/ -e cpu_atom/r1085/ -c 30001";
const char kPerfLLCMissesCmd[] = "-- record -a -e r412e -g -c 30007";
const char kPerfLLCMissesPreciseCmd[] = "-- record -a -e r412e:pp -g -c 30007";
const char kPerfDTLBMissesDAPGoldmont[] =
    "-- record -a -e mem_uops_retired.dtlb_miss_loads:pp -c 2003 -d";
const char kPerfDTLBMissesDAPTremont[] = "-- record -a -e r11d0:pp -c 2003 -d";
const char kPerfDTLBMissesDAPHaswell[] =
    "-- record -a -e mem_uops_retired.stlb_miss_loads:pp -c 2003 -d";
const char kPerfDTLBMissesDAPSkylake[] =
    "-- record -a -e mem_inst_retired.stlb_miss_loads:pp -c 2003 -d";

const char kPerfETMCmd[] =
    "--run_inject --inject_args inject;--itrace=i512il;--strip -- record -a -e "
    "cs_etm/autofdo/";

// Converts a protobuf to serialized format as a byte vector.
std::vector<uint8_t> SerializeMessageToVector(
    const google::protobuf::MessageLite& message) {
  std::vector<uint8_t> result(message.ByteSize());
  message.SerializeToArray(result.data(), result.size());
  return result;
}

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

base::TimeDelta GetDuration(const std::vector<std::string>& quipper_args) {
  for (auto it = quipper_args.begin(); it != quipper_args.end(); ++it) {
    if (*it == "--duration" && it != quipper_args.end()) {
      int dur;
      if (base::StringToInt(*(it + 1), &dur))
        return base::Seconds(dur);
    }
  }
  return base::Seconds(0);
}

// A mock PerfOutputCall class for testing, which outputs example perf data
// after the profile duration elapses.
class FakePerfOutputCall : public PerfOutputCall {
 public:
  using PerfOutputCall::DoneCallback;
  FakePerfOutputCall(base::TimeDelta duration,
                     DoneCallback done_callback,
                     base::OnceClosure on_stop)
      : PerfOutputCall(),
        done_callback_(std::move(done_callback)),
        on_stop_(std::move(on_stop)) {
    // Simulates collection done after profiling duration.
    collection_done_timer_.Start(FROM_HERE, duration, this,
                                 &FakePerfOutputCall::OnCollectionDone);
  }

  FakePerfOutputCall(const FakePerfOutputCall&) = delete;
  FakePerfOutputCall& operator=(const FakePerfOutputCall&) = delete;

  ~FakePerfOutputCall() override = default;

  void Stop() override {
    // Notify the observer that Stop() is called.
    std::move(on_stop_).Run();

    // Simulates that collection is done when we stop the perf session. Note
    // that this may destroy |this| and should be the last action in Stop().
    if (collection_done_timer_.IsRunning())
      collection_done_timer_.FireNow();
  }

 private:
  void OnCollectionDone() {
    std::move(done_callback_)
        .Run(GetExamplePerfDataProto().SerializeAsString());
  }

  DoneCallback done_callback_;
  base::OneShotTimer collection_done_timer_;
  base::OnceClosure on_stop_;
};

// Allows testing of PerfCollector behavior when an incognito window is opened.
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

  TestIncognitoObserver(const TestIncognitoObserver&) = delete;
  TestIncognitoObserver& operator=(const TestIncognitoObserver&) = delete;

  bool IncognitoLaunched() const override { return incognito_launched_; }

 private:
  explicit TestIncognitoObserver(bool incognito_launched)
      : WindowedIncognitoObserver(nullptr, 0),
        incognito_launched_(incognito_launched) {}

  bool incognito_launched_;
};

// Allows access to some private methods for testing.
class TestPerfCollector : public PerfCollector {
 public:
  TestPerfCollector() = default;

  TestPerfCollector(const TestPerfCollector&) = delete;
  TestPerfCollector& operator=(const TestPerfCollector&) = delete;

  using MetricCollector::CollectionAttemptStatus;
  using MetricCollector::CollectPerfDataAfterSessionRestore;
  using MetricCollector::OnJankStarted;
  using MetricCollector::OnJankStopped;
  using MetricCollector::ShouldCollect;
  using MetricCollector::StopTimer;
  using PerfCollector::AddCachedDataDelta;
  using PerfCollector::collection_params;
  using PerfCollector::CollectPSICPU;
  using PerfCollector::command_selector;
  using PerfCollector::CommandEventType;
  using PerfCollector::EventType;
  using PerfCollector::Init;
  using PerfCollector::IsRunning;
  using PerfCollector::LacrosChannelAndVersion;
  using PerfCollector::max_frequencies_mhz;
  using PerfCollector::ParseLacrosPath;
  using PerfCollector::ParseOutputProtoIfValid;
  using PerfCollector::ParsePSICPUStatus;
  using PerfCollector::RecordUserLogin;
  using PerfCollector::set_profile_done_callback;

  bool collection_stopped() { return collection_stopped_; }
  bool collection_done() { return !real_callback_; }

  base::TimeDelta elapsed_duration;

 protected:
  std::unique_ptr<PerfOutputCall> CreatePerfOutputCall(
      const std::vector<std::string>& quipper_args,
      bool disable_cpu_idle,
      PerfOutputCall::DoneCallback callback) override {
    real_callback_ = std::move(callback);
    elapsed_duration = GetDuration(quipper_args);

    return std::make_unique<FakePerfOutputCall>(
        elapsed_duration,
        base::BindOnce(&TestPerfCollector::OnCollectionDone,
                       base::Unretained(this)),
        base::BindOnce(&TestPerfCollector::OnCollectionStopped,
                       base::Unretained(this)));
  }

  void OnCollectionDone(std::string perf_output) {
    std::move(real_callback_).Run(std::move(perf_output));
  }

  void OnCollectionStopped() { collection_stopped_ = true; }

  PerfOutputCall::DoneCallback real_callback_;
  bool collection_stopped_ = false;
};

const base::TimeDelta kPeriodicCollectionInterval = base::Hours(1);
const base::TimeDelta kCollectionDuration = base::Seconds(2);

// A wrapper around CommandEventType, to test if a perf command samples
// the cycles event. The wrapper takes a command as a string, while the
// wrapped CommandEventType takes the command split into words.
bool DoesCommandSampleCycles(std::string command) {
  using EventType = TestPerfCollector::EventType;
  std::vector<std::string> cmd_args =
      base::SplitString(command, kPerfCommandDelimiter, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  return TestPerfCollector::CommandEventType(cmd_args) == EventType::kCycles;
}

// A wrapper around CommandEventType, to test if a perf command samples
// the etm event. The wrapper takes a command as a string, while the
// wrapped CommandEventType takes the command split into words.
bool DoesCommandSampleETM(std::string command) {
  using EventType = TestPerfCollector::EventType;
  std::vector<std::string> cmd_args =
      base::SplitString(command, kPerfCommandDelimiter, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  return TestPerfCollector::CommandEventType(cmd_args) == EventType::kETM;
}

}  // namespace

class PerfCollectorTest : public testing::Test {
 public:
  PerfCollectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  PerfCollectorTest(const PerfCollectorTest&) = delete;
  PerfCollectorTest& operator=(const PerfCollectorTest&) = delete;

  void SaveProfile(std::unique_ptr<SampledProfile> sampled_profile) {
    cached_profile_data_.resize(cached_profile_data_.size() + 1);
    cached_profile_data_.back().Swap(sampled_profile.get());
  }

  void SetUp() override {
    perf_collector_ = std::make_unique<TestPerfCollector>();
    // Set the periodic collection delay to a well known quantity, so we can
    // fast forward the time.
    perf_collector_->collection_params().periodic_interval =
        kPeriodicCollectionInterval;
    // Set collection duration to a known quantity for fast forwarding time.
    perf_collector_->collection_params().collection_duration =
        kCollectionDuration;

    perf_collector_->set_profile_done_callback(base::BindRepeating(
        &PerfCollectorTest::SaveProfile, base::Unretained(this)));

    perf_collector_->Init();
    // PerfCollector requires the user to be logged in.
    perf_collector_->RecordUserLogin(base::TimeTicks::Now());

    perf_collector_->elapsed_duration = base::Seconds(0);
  }

  void TearDown() override {
    perf_collector_.reset();
    cached_profile_data_.clear();
  }

 protected:
  // task_environment_ must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  std::vector<SampledProfile> cached_profile_data_;

  std::unique_ptr<TestPerfCollector> perf_collector_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PerfCollectorTest, CheckSetup) {
  // No profiles saved at start.
  EXPECT_TRUE(cached_profile_data_.empty());

  EXPECT_FALSE(TestIncognitoObserver::CreateWithIncognitoLaunched(false)
                   ->IncognitoLaunched());
  EXPECT_TRUE(TestIncognitoObserver::CreateWithIncognitoLaunched(true)
                  ->IncognitoLaunched());
  task_environment_.RunUntilIdle();
  EXPECT_GT(perf_collector_->max_frequencies_mhz().size(), 0u);
}

TEST_F(PerfCollectorTest, PrependDuration) {
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(perf_collector_->IsRunning());
  base::HistogramTester histogram_tester;

  // Advance the clock by a periodic collection interval to trigger
  // a collection.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  EXPECT_EQ(perf_collector_->elapsed_duration, kCollectionDuration);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.CollectPerf",
      TestPerfCollector::CollectionAttemptStatus::SUCCESS, 1);
}

TEST_F(PerfCollectorTest, NoCollectionWhenProfileCacheFull) {
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(perf_collector_->IsRunning());
  // Pretend the cache is full.
  perf_collector_->AddCachedDataDelta(4 * 1024 * 1024);
  base::HistogramTester histogram_tester;

  // Advance the clock by a periodic collection interval. We shouldn't find a
  // profile because the cache is full.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  EXPECT_TRUE(cached_profile_data_.empty());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.CollectPerf",
      TestPerfCollector::CollectionAttemptStatus::NOT_READY_TO_COLLECT, 1);
}

// Simulate opening and closing of incognito window in between calls to
// ParseOutputProtoIfValid().
TEST_F(PerfCollectorTest, IncognitoWindowOpened) {
  PerfDataProto perf_data_proto = GetExamplePerfDataProto();
  EXPECT_GT(perf_data_proto.ByteSize(), 0);
  task_environment_.RunUntilIdle();

  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  auto incognito_observer =
      TestIncognitoObserver::CreateWithIncognitoLaunched(false);
  perf_collector_->ParseOutputProtoIfValid(std::move(incognito_observer),
                                           std::move(sampled_profile), true,
                                           perf_data_proto.SerializeAsString());

  // Run the BrowserTaskEnvironment queue until it's empty as the above
  // ParseOutputProtoIfValid call posts a task to asynchronously collect process
  // and thread types and the profile cache will be updated asynchronously via
  // another PostTask request after this collection completes.
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile1 = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile1.trigger_event());
  EXPECT_TRUE(profile1.has_ms_after_login());
  ASSERT_TRUE(profile1.has_perf_data());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto),
            SerializeMessageToVector(profile1.perf_data()));
  EXPECT_GT(profile1.cpu_max_frequency_mhz_size(), 0);
  cached_profile_data_.clear();

  base::HistogramTester histogram_tester;
  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  // An incognito window opens.
  incognito_observer = TestIncognitoObserver::CreateWithIncognitoLaunched(true);
  perf_collector_->ParseOutputProtoIfValid(std::move(incognito_observer),
                                           std::move(sampled_profile), true,
                                           perf_data_proto.SerializeAsString());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(cached_profile_data_.empty());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.CollectPerf",
      TestPerfCollector::CollectionAttemptStatus::INCOGNITO_LAUNCHED, 1);

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  sampled_profile->set_suspend_duration_ms(60000);
  sampled_profile->set_ms_after_resume(1500);
  // Incognito window closes.
  incognito_observer =
      TestIncognitoObserver::CreateWithIncognitoLaunched(false);
  perf_collector_->ParseOutputProtoIfValid(std::move(incognito_observer),
                                           std::move(sampled_profile), true,
                                           perf_data_proto.SerializeAsString());
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1U, cached_profile_data_.size());

  const SampledProfile& profile2 = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile2.trigger_event());
  EXPECT_TRUE(profile2.has_ms_after_login());
  EXPECT_EQ(60000, profile2.suspend_duration_ms());
  EXPECT_EQ(1500, profile2.ms_after_resume());
  ASSERT_TRUE(profile2.has_perf_data());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto),
            SerializeMessageToVector(profile2.perf_data()));
  EXPECT_GT(profile2.cpu_max_frequency_mhz_size(), 0);
}

TEST_F(PerfCollectorTest, CollectPSICPUDataSuccess) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath psi_cpu_file = tmp_dir.GetPath().Append("psi_cpu");
  ASSERT_TRUE(base::WriteFile(
      psi_cpu_file, "some avg10=2.04 avg60=0.75 avg300=0.40 total=1576"));

  base::HistogramTester histogram_tester;

  auto sampled_profile = std::make_unique<SampledProfile>();
  TestPerfCollector::CollectPSICPU(sampled_profile.get(), psi_cpu_file.value());

  EXPECT_FLOAT_EQ(sampled_profile->psi_cpu_last_10s_pct(), 2.04);
  EXPECT_FLOAT_EQ(sampled_profile->psi_cpu_last_60s_pct(), 0.75);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.ParsePSICPU",
      TestPerfCollector::ParsePSICPUStatus::kSuccess, 1);
}

TEST_F(PerfCollectorTest, CollectPSICPUDataReadFileFailed) {
  const char kPSICPUPath[] = "/some/random/path";
  base::HistogramTester histogram_tester;

  auto sampled_profile = std::make_unique<SampledProfile>();
  TestPerfCollector::CollectPSICPU(sampled_profile.get(), kPSICPUPath);

  EXPECT_FALSE(sampled_profile->has_psi_cpu_last_10s_pct());
  EXPECT_FALSE(sampled_profile->has_psi_cpu_last_60s_pct());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.ParsePSICPU",
      TestPerfCollector::ParsePSICPUStatus::kReadFileFailed, 1);
}

TEST_F(PerfCollectorTest, CollectPSICPUDataUnexpectedDataFormat) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath psi_cpu_file = tmp_dir.GetPath().Append("psi_cpu");
  ASSERT_TRUE(base::WriteFile(psi_cpu_file, "random content"));

  base::HistogramTester histogram_tester;

  auto sampled_profile = std::make_unique<SampledProfile>();
  TestPerfCollector::CollectPSICPU(sampled_profile.get(), psi_cpu_file.value());

  EXPECT_FALSE(sampled_profile->has_psi_cpu_last_10s_pct());
  EXPECT_FALSE(sampled_profile->has_psi_cpu_last_60s_pct());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.ParsePSICPU",
      TestPerfCollector::ParsePSICPUStatus::kUnexpectedDataFormat, 1);
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_IvyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x3a;  // IvyBridge
  cpuid.model_name = "";
  cpuid.release = "3.8.11";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdIvyBridge,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_SandyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2a;  // SandyBridge
  cpuid.model_name = "";
  cpuid.release = "3.8.11";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdIvyBridge,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Haswell) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x45;  // Haswell
  cpuid.model_name = "";
  cpuid.release = "3.8.11";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  // No LBR callstacks because the kernel is old.
  EXPECT_FALSE(base::Contains(cmds, kPerfLBRCallgraphCmd,
                              &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdIvyBridge,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPHaswell,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Skylake) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x4E;  // Skylake
  cpuid.model_name = "";
  cpuid.release = "3.18.0";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 3UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  // We have both FP and LBR based callstacks.
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[2].value, kPerfLBRCallgraphCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdSkylake,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPSkylake,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Tigerlake) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x8C;  // Tigerlake
  cpuid.model_name = "";
  cpuid.release = "5.4.64";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 3UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesPPPHGCmd);
  // We have both FP and LBR based callstacks.
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[2].value, kPerfLBRCallgraphPPPCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdSkylake,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPSkylake,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Goldmont) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x5c;  // Goldmont
  cpuid.model_name = "";
  cpuid.release = "4.4.196";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  // No LBR callstacks because the microarchitecture doesn't support it.
  EXPECT_FALSE(base::Contains(cmds, kPerfLBRCallgraphCmd,
                              &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmdAtom,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesPreciseCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdAtom,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPGoldmont,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_GoldmontPlus) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x7a;  // GoldmontPlus
  cpuid.model_name = "";
  cpuid.release = "4.14.214";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  // No LBR callstacks because the microarchitecture doesn't support it.
  EXPECT_FALSE(base::Contains(cmds, kPerfLBRCallgraphCmd,
                              &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmdAtom,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesPreciseCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdSkylake,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPGoldmont,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Tremont) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x9c;  // Tremont
  cpuid.model_name = "";
  cpuid.release = "5.4.206";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCallgraphPPPCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmdTremont,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesPreciseCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdTremont,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPTremont,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_AlderLake) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x9a;  // AlderLake
  cpuid.model_name = "";
  cpuid.release = "6.6.30";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCallgraphPPPCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmdAlderLake,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesPreciseCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdAlderLake,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPTremont,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Gracemont) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0xbe;  // Gracemont
  cpuid.model_name = "";
  cpuid.release = "6.6.30";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphPPPHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCallgraphPPPCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLBRCmdTremont,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfLLCMissesPreciseCmd,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfITLBMissCyclesCmdTremont,
                             &RandomSelector::WeightAndValue::value));
  EXPECT_TRUE(base::Contains(cmds, kPerfDTLBMissesDAPTremont,
                             &RandomSelector::WeightAndValue::value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Excavator) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "AuthenticAMD";
  cpuid.family = 0x15;
  cpuid.model = 0x70;  // Excavator
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_FALSE(base::Contains(cmds, kPerfLLCMissesCmd,
                              &RandomSelector::WeightAndValue::value))
      << "Excavator does not support this command";
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_Arm32) {
  CPUIdentity cpuid;
  cpuid.arch = "armv7l";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_FALSE(
      base::Contains(cmds, kPerfLBRCmd, &RandomSelector::WeightAndValue::value))
      << "ARM32 does not support this command";
  EXPECT_FALSE(base::Contains(cmds, kPerfLLCMissesCmd,
                              &RandomSelector::WeightAndValue::value))
      << "ARM32 does not support this command";
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_Arm64) {
  CPUIdentity cpuid;
  cpuid.arch = "aarch64";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_FALSE(
      base::Contains(cmds, kPerfLBRCmd, &RandomSelector::WeightAndValue::value))
      << "ARM64 does not support this command";
  EXPECT_FALSE(base::Contains(cmds, kPerfLLCMissesCmd,
                              &RandomSelector::WeightAndValue::value))
      << "ARM64 does not support this command";
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_Arm64_ETM) {
  feature_list_.InitAndEnableFeature(kCWPCollectsETM);
  CPUIdentity cpuid;
  cpuid.arch = "aarch64";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "TROGDOR");
  ASSERT_GE(cmds.size(), 3UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_EQ(cmds[2].value, kPerfETMCmd);
  EXPECT_TRUE(DoesCommandSampleETM(cmds[2].value));
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_x86_32) {
  CPUIdentity cpuid;
  cpuid.arch = "x86";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2f;  // Westmere
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
  EXPECT_EQ(cmds[1].value, kPerfFPCallgraphHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[1].value));
  EXPECT_FALSE(
      base::Contains(cmds, kPerfLBRCmd, &RandomSelector::WeightAndValue::value))
      << "x86_32 does not support this command";
  EXPECT_FALSE(base::Contains(cmds, kPerfLLCMissesCmd,
                              &RandomSelector::WeightAndValue::value))
      << "x86_32 does not support this command";
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_Unknown) {
  CPUIdentity cpuid;
  cpuid.arch = "nonsense";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpuModel(cpuid, "");
  EXPECT_EQ(1UL, cmds.size());
  EXPECT_EQ(cmds[0].value, kPerfCyclesHGCmd);
  EXPECT_TRUE(DoesCommandSampleCycles(cmds[0].value));
}

TEST_F(PerfCollectorTest, CommandMatching_Empty) {
  CPUIdentity cpuid = {};
  std::map<std::string, std::string> params;
  EXPECT_EQ("", internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_NoPerfCommands) {
  CPUIdentity cpuid = {};
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("NotEvenClose", ""));
  params.insert(std::make_pair("NotAPerfCommand", ""));
  params.insert(std::make_pair("NotAPerfCommand::Really", ""));
  params.insert(std::make_pair("NotAPerfCommand::Nope::0", ""));
  params.insert(std::make_pair("PerfCommands::SoClose::0", ""));
  EXPECT_EQ("", internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_NoMatch) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 6;
  cpuid.model = 0x3a;  // IvyBridge
  cpuid.model_name = "Xeon or somesuch";
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("PerfCommand::armv7l::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86::1", "perf command"));
  params.insert(std::make_pair("PerfCommand::Broadwell::0", "perf command"));

  EXPECT_EQ("", internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_default) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 6;
  cpuid.model = 0x3a;  // IvyBridge
  cpuid.model_name = "Xeon or somesuch";
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("PerfCommand::default::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::armv7l::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86::1", "perf command"));
  params.insert(std::make_pair("PerfCommand::Broadwell::0", "perf command"));

  EXPECT_EQ("default", internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_SystemArch) {
  CPUIdentity cpuid;
  cpuid.arch = "nothing_in_particular";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("PerfCommand::default::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::armv7l::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86::1", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86_64::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86_64::xyz#$%", "perf command"));
  params.insert(std::make_pair("PerfCommand::Broadwell::0", "perf command"));

  EXPECT_EQ("default", internal::FindBestCpuSpecifierFromParams(params, cpuid));

  cpuid.arch = "armv7l";
  EXPECT_EQ("armv7l", internal::FindBestCpuSpecifierFromParams(params, cpuid));

  cpuid.arch = "x86";
  EXPECT_EQ("x86", internal::FindBestCpuSpecifierFromParams(params, cpuid));

  cpuid.arch = "x86_64";
  EXPECT_EQ("x86_64", internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_Microarchitecture) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 6;
  cpuid.model = 0x3D;  // Broadwell
  cpuid.model_name = "Wrong Model CPU @ 0 Hz";
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("PerfCommand::default::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86_64::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::Broadwell::0", "perf command"));
  params.insert(
      std::make_pair("PerfCommand::interesting-model-500x::0", "perf command"));

  EXPECT_EQ("Broadwell",
            internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_SpecificModel) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 6;
  cpuid.model = 0x3D;  // Broadwell
  cpuid.model_name = "An Interesting(R) Model(R) 500x CPU @ 1.2GHz";
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("PerfCommand::default::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86_64::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::Broadwell::0", "perf command"));
  params.insert(
      std::make_pair("PerfCommand::interesting-model-500x::0", "perf command"));

  EXPECT_EQ("interesting-model-500x",
            internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

TEST_F(PerfCollectorTest, CommandMatching_SpecificModel_LongestMatch) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 6;
  cpuid.model = 0x3D;  // Broadwell
  cpuid.model_name = "An Interesting(R) Model(R) 500x CPU @ 1.2GHz";
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("PerfCommand::default::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::x86_64::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::Broadwell::0", "perf command"));
  params.insert(std::make_pair("PerfCommand::model-500x::0", "perf command"));
  params.insert(
      std::make_pair("PerfCommand::interesting-model-500x::0", "perf command"));
  params.insert(
      std::make_pair("PerfCommand::interesting-model::0", "perf command"));

  EXPECT_EQ("interesting-model-500x",
            internal::FindBestCpuSpecifierFromParams(params, cpuid));
}

// Testing that jankiness collection trigger doesn't interfere with an ongoing
// collection.
TEST_F(PerfCollectorTest, StopCollection_AnotherTrigger) {
  const int kRestoredTabs = 1;

  perf_collector_->CollectPerfDataAfterSessionRestore(base::Seconds(1),
                                                      kRestoredTabs);
  // Timer is active after the OnSessionRestoreDone call.
  EXPECT_TRUE(perf_collector_->IsRunning());
  // A collection in action: should reject another collection request.
  EXPECT_FALSE(perf_collector_->ShouldCollect());

  task_environment_.FastForwardBy(base::Milliseconds(100));
  // A collection is ongoing. Triggering a jankiness collection should have no
  // effect on the existing collection.
  perf_collector_->OnJankStarted();
  task_environment_.FastForwardBy(base::Milliseconds(100));
  // This doesn't stop the existing collection.
  perf_collector_->OnJankStopped();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(perf_collector_->collection_done());
  EXPECT_FALSE(perf_collector_->collection_stopped());

  // Fast forward time past the collection duration to complete the collection.
  task_environment_.FastForwardBy(
      perf_collector_->collection_params().collection_duration);
  // The collection finishes automatically without being stopped.
  EXPECT_FALSE(perf_collector_->collection_stopped());
  EXPECT_TRUE(perf_collector_->collection_done());

  ASSERT_EQ(1U, cached_profile_data_.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(perf_collector_->IsRunning());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
  EXPECT_EQ(kRestoredTabs, profile.num_tabs_restored());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());
}

// Test stopping a jankiness collection.
TEST_F(PerfCollectorTest, JankinessCollectionStopped) {
  EXPECT_TRUE(perf_collector_->ShouldCollect());
  perf_collector_->OnJankStarted();
  // A collection in action: should reject another collection request.
  EXPECT_FALSE(perf_collector_->ShouldCollect());

  task_environment_.FastForwardBy(base::Milliseconds(100));

  perf_collector_->OnJankStopped();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(perf_collector_->collection_done());
  EXPECT_TRUE(perf_collector_->collection_stopped());

  ASSERT_EQ(1U, cached_profile_data_.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(perf_collector_->IsRunning());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::JANKY_TASK, profile.trigger_event());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());
}

// Test a jankiness collection is done when the collection duration elapses.
TEST_F(PerfCollectorTest, JankinessCollectionDurationElapsed) {
  EXPECT_TRUE(perf_collector_->ShouldCollect());
  perf_collector_->OnJankStarted();
  // A collection in action: should reject another collection request.
  EXPECT_FALSE(perf_collector_->ShouldCollect());

  // The jank lasts for 2 collection durations. The collection should be done
  // before the jank stops.
  task_environment_.FastForwardBy(
      2 * perf_collector_->collection_params().collection_duration);
  // The collection is done without being stopped.
  EXPECT_TRUE(perf_collector_->collection_done());
  EXPECT_FALSE(perf_collector_->collection_stopped());

  ASSERT_EQ(1U, cached_profile_data_.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(perf_collector_->IsRunning());

  const SampledProfile& profile = cached_profile_data_[0];
  EXPECT_EQ(SampledProfile::JANKY_TASK, profile.trigger_event());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  perf_collector_->OnJankStopped();
  task_environment_.RunUntilIdle();
  // The arrival of OnJankStopped() has no effect on PerfCollector after the
  // collection is done.
  EXPECT_TRUE(perf_collector_->collection_done());
  EXPECT_FALSE(perf_collector_->collection_stopped());
}

TEST_F(PerfCollectorTest, LacrosPathRootfs) {
  base::HistogramTester histogram_tester;
  const char rootfs_path[] = "/run/lacros/chrome";
  metrics::SystemProfileProto_Channel rootfs_lacros_channel;
  std::string rootfs_lacros_version;
  EXPECT_FALSE(TestPerfCollector::LacrosChannelAndVersion(
      rootfs_path, rootfs_lacros_channel, rootfs_lacros_version));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.ParseLacrosPath",
      TestPerfCollector::ParseLacrosPath::kRootfs, 1);
}

TEST_F(PerfCollectorTest, LacrosChannelAndVersion) {
  base::HistogramTester histogram_tester;

  const char stable_path[] =
      "/run/imageloader/lacros-dogfood-stable/95.0.4623.2/chrome";
  metrics::SystemProfileProto_Channel stable_channel;
  std::string stable_version;
  EXPECT_TRUE(TestPerfCollector::LacrosChannelAndVersion(
      stable_path, stable_channel, stable_version));
  EXPECT_EQ(stable_channel, metrics::SystemProfileProto_Channel_CHANNEL_STABLE);
  EXPECT_EQ(stable_version, "95.0.4623.2");

  const char beta_path[] =
      "/run/imageloader/lacros-dogfood-beta/97.0.4623.2/chrome";
  metrics::SystemProfileProto_Channel beta_channel;
  std::string beta_version;
  EXPECT_TRUE(TestPerfCollector::LacrosChannelAndVersion(
      beta_path, beta_channel, beta_version));
  EXPECT_EQ(beta_channel, metrics::SystemProfileProto_Channel_CHANNEL_BETA);
  EXPECT_EQ(beta_version, "97.0.4623.2");

  const char dev_path[] =
      "/run/imageloader/lacros-dogfood-dev/99.0.4623.2/chrome";
  metrics::SystemProfileProto_Channel dev_channel;
  std::string dev_version;
  EXPECT_TRUE(TestPerfCollector::LacrosChannelAndVersion(dev_path, dev_channel,
                                                         dev_version));
  EXPECT_EQ(dev_channel, metrics::SystemProfileProto_Channel_CHANNEL_DEV);
  EXPECT_EQ(dev_version, "99.0.4623.2");

  const char canary_path[] =
      "/run/imageloader/lacros-dogfood-canary/100.0.4623.2/chrome";
  metrics::SystemProfileProto_Channel canary_channel;
  std::string canary_version;
  EXPECT_TRUE(TestPerfCollector::LacrosChannelAndVersion(
      canary_path, canary_channel, canary_version));
  EXPECT_EQ(canary_channel, metrics::SystemProfileProto_Channel_CHANNEL_CANARY);
  EXPECT_EQ(canary_version, "100.0.4623.2");

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.ParseLacrosPath",
      TestPerfCollector::ParseLacrosPath::kStateful, 4);
}

TEST_F(PerfCollectorTest, LacrosPathUnrecognized) {
  base::HistogramTester histogram_tester;
  const char unrecognized_path[] = "/run/imageloader/lacros/chrome";
  metrics::SystemProfileProto_Channel unrecognized_channel;
  std::string unrecognized_version;
  EXPECT_FALSE(TestPerfCollector::LacrosChannelAndVersion(
      unrecognized_path, unrecognized_channel, unrecognized_version));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.CWP.ParseLacrosPath",
      TestPerfCollector::ParseLacrosPath::kUnrecognized, 1);
}

TEST_F(PerfCollectorTest, CommandEventType) {
  using EventType = TestPerfCollector::EventType;
  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "record", "-a", "-e", "cycles",
                                                 "-c", "1000003"}),
            EventType::kCycles);
  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "record", "-a", "-e", "cycles",
                                                 "-g", "-c", "4000037"}),
            EventType::kCycles);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "cycles", "-c",
                 "4000037", "--call-graph", "lbr"}),
            EventType::kCycles);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "cycles:ppp",
                 "-c", "1000003"}),
            EventType::kCycles);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "cycles:ppp",
                 "-g", "-c", "4000037"}),
            EventType::kCycles);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "cycles:ppp",
                 "-c", "4000037", "--call-graph", "lbr"}),
            EventType::kCycles);

  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "record", "-a", "-e", "r20c4",
                                                 "-b", "-c", "200011"}),
            EventType::kOther);
  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "record", "-a", "-e", "rc4",
                                                 "-b", "-c", "300001"}),
            EventType::kOther);
  EXPECT_EQ(
      TestPerfCollector::CommandEventType({"--duration", "0", "--", "record",
                                           "-a", "-e", "r0481", "-c", "2003"}),
      EventType::kOther);
  EXPECT_EQ(
      TestPerfCollector::CommandEventType({"--duration", "0", "--", "record",
                                           "-a", "-e", "r13d0", "-c", "2003"}),
      EventType::kOther);
  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "record", "-a", "-e",
                                                 "iTLB-misses", "-c", "2003"}),
            EventType::kOther);
  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "record", "-a", "-e",
                                                 "dTLB-misses", "-c", "2003"}),
            EventType::kOther);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "cache-misses",
                 "-c", "10007"}),
            EventType::kOther);

  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "instructions",
                 "-e", "cycles", "-c", "1000003"}),
            EventType::kCycles);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-a", "-e", "instructions",
                 "-e", "cycles:ppp", "-c", "1000003"}),
            EventType::kCycles);

  EXPECT_EQ(TestPerfCollector::CommandEventType({"--duration", "0", "--",
                                                 "stat", "-a", "-e", "cycles",
                                                 "-e", "instructions"}),
            EventType::kOther);

  EXPECT_EQ(
      TestPerfCollector::CommandEventType(
          {"--duration", "0", "--", "record", "-e", "cs_etm/autofdo/", "-a"}),
      EventType::kETM);
  EXPECT_EQ(
      TestPerfCollector::CommandEventType(
          {"--duration", "0", "--", "record", "-e", "cs_etm/autofdo/u", "-a"}),
      EventType::kETM);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--", "record", "-e",
                 "cs_etm/autofdo,preset=1/", "-a"}),
            EventType::kETM);
  EXPECT_EQ(TestPerfCollector::CommandEventType(
                {"--duration", "0", "--run_inject", "--inject_args", "-b", "--",
                 "record", "-e", "cs_etm/autofdo/", "-a"}),
            EventType::kETM);
}

class PerfCollectorCollectionParamsTest : public testing::Test {
 public:
  PerfCollectorCollectionParamsTest() {}

  PerfCollectorCollectionParamsTest(const PerfCollectorCollectionParamsTest&) =
      delete;
  PerfCollectorCollectionParamsTest& operator=(
      const PerfCollectorCollectionParamsTest&) = delete;

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PerfCollectorCollectionParamsTest, Commands_InitializedAfterVariations) {
  auto perf_collector = std::make_unique<TestPerfCollector>();
  EXPECT_TRUE(perf_collector->command_selector().odds().empty());
  // Init would be called after VariationsService is initialized.
  perf_collector->Init();
  EXPECT_FALSE(perf_collector->command_selector().odds().empty());
}

TEST_F(PerfCollectorCollectionParamsTest, Commands_EmptyExperiment) {
  std::vector<RandomSelector::WeightAndValue> default_cmds =
      internal::GetDefaultCommandsForCpuModel(
          GetCPUIdentity(), base::SysInfo::HardwareModelName());
  std::map<std::string, std::string> params;
  ASSERT_TRUE(base::AssociateFieldTrialParams("ChromeOSWideProfilingCollection",
                                              "group_name", params));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "ChromeOSWideProfilingCollection", "group_name"));

  auto perf_collector = std::make_unique<TestPerfCollector>();
  EXPECT_TRUE(perf_collector->command_selector().odds().empty());
  perf_collector->Init();
  EXPECT_EQ(default_cmds, perf_collector->command_selector().odds());
}

TEST_F(PerfCollectorCollectionParamsTest, Commands_InvalidValues) {
  std::vector<RandomSelector::WeightAndValue> default_cmds =
      internal::GetDefaultCommandsForCpuModel(
          GetCPUIdentity(), base::SysInfo::HardwareModelName());
  std::map<std::string, std::string> params;
  // Use the "default" cpu specifier since we don't want to predict what CPU
  // this test is running on. (CPU detection is tested above.)
  params.insert(std::make_pair("PerfCommand::default::0", ""));
  params.insert(std::make_pair("PerfCommand::default::1", " "));
  params.insert(std::make_pair("PerfCommand::default::2", " leading space"));
  params.insert(
      std::make_pair("PerfCommand::default::3", "no-spaces-or-numbers"));
  params.insert(
      std::make_pair("PerfCommand::default::4", "NaN-trailing-space "));
  params.insert(std::make_pair("PerfCommand::default::5", "NaN x"));
  params.insert(std::make_pair("PerfCommand::default::6", "perf command"));
  ASSERT_TRUE(base::AssociateFieldTrialParams("ChromeOSWideProfilingCollection",
                                              "group_name", params));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "ChromeOSWideProfilingCollection", "group_name"));

  auto perf_collector = std::make_unique<TestPerfCollector>();
  EXPECT_TRUE(perf_collector->command_selector().odds().empty());
  perf_collector->Init();
  EXPECT_EQ(default_cmds, perf_collector->command_selector().odds());
}

TEST_F(PerfCollectorCollectionParamsTest, Commands_Override) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<RandomSelector::WeightAndValue> default_cmds =
      internal::GetDefaultCommandsForCpuModel(
          GetCPUIdentity(), base::SysInfo::HardwareModelName());
  std::map<std::string, std::string> params;
  // Use the "default" cpu specifier since we don't want to predict what CPU
  // this test is running on. (CPU detection is tested above.)
  params.insert(
      std::make_pair("PerfCommand::default::0", "50 perf record foo"));
  params.insert(
      std::make_pair("PerfCommand::default::1", "25 perf record bar"));
  params.insert(
      std::make_pair("PerfCommand::default::2", "25 perf record baz"));
  params.insert(
      std::make_pair("PerfCommand::another-cpu::0", "7 perf record bar"));
  ASSERT_TRUE(base::AssociateFieldTrialParams("ChromeOSWideProfilingCollection",
                                              "group_name", params));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "ChromeOSWideProfilingCollection", "group_name"));

  auto perf_collector = std::make_unique<TestPerfCollector>();
  EXPECT_TRUE(perf_collector->command_selector().odds().empty());
  perf_collector->Init();

  std::vector<WeightAndValue> expected_cmds;
  expected_cmds.push_back(WeightAndValue(50.0, "perf record foo"));
  expected_cmds.push_back(WeightAndValue(25.0, "perf record bar"));
  expected_cmds.push_back(WeightAndValue(25.0, "perf record baz"));

  EXPECT_EQ(expected_cmds, perf_collector->command_selector().odds());
}

TEST_F(PerfCollectorCollectionParamsTest, Parameters_Override) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("ProfileCollectionDurationSec", "15"));
  params.insert(std::make_pair("PeriodicProfilingIntervalMs", "3600000"));
  params.insert(std::make_pair("ResumeFromSuspend::SamplingFactor", "1"));
  params.insert(std::make_pair("ResumeFromSuspend::MaxDelaySec", "10"));
  params.insert(std::make_pair("RestoreSession::SamplingFactor", "2"));
  params.insert(std::make_pair("RestoreSession::MaxDelaySec", "20"));
  ASSERT_TRUE(base::AssociateFieldTrialParams("ChromeOSWideProfilingCollection",
                                              "group_name", params));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "ChromeOSWideProfilingCollection", "group_name"));

  auto perf_collector = std::make_unique<TestPerfCollector>();
  const auto& parsed_params = perf_collector->collection_params();

  // Not initialized yet:
  EXPECT_NE(base::Seconds(15), parsed_params.collection_duration);
  EXPECT_NE(base::Hours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::Seconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::Seconds(20),
            parsed_params.restore_session.max_collection_delay);

  perf_collector->Init();

  EXPECT_EQ(base::Seconds(15), parsed_params.collection_duration);
  EXPECT_EQ(base::Hours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::Seconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::Seconds(20),
            parsed_params.restore_session.max_collection_delay);
}

}  // namespace metrics
