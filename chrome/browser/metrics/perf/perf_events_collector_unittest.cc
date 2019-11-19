// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_events_collector.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/metrics/perf/cpu_identity.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

const char kPerfRecordCyclesCmd[] = "perf record -a -e cycles -c 1000003";
const char kPerfRecordCallgraphCmd[] = "perf record -a -e cycles -g -c 4000037";
const char kPerfRecordLBRCmd[] = "perf record -a -e r20c4 -b -c 200011";
const char kPerfRecordLBRCmdAtom[] = "perf record -a -e rc4 -b -c 300001";
const char kPerfRecordDataTLBMissesCmdGLM[] = "perf record -a -e r13d0 -c 2003";
const char kPerfRecordDataTLBMissesCmd[] =
    "perf record -a -e dTLB-misses -c 2003";
const char kPerfRecordCacheMissesCmd[] =
    "perf record -a -e cache-misses -c 10007";

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

  DISALLOW_COPY_AND_ASSIGN(FakePerfOutputCall);
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

  bool IncognitoLaunched() const override { return incognito_launched_; }

 private:
  explicit TestIncognitoObserver(bool incognito_launched)
      : WindowedIncognitoObserver(nullptr, 0),
        incognito_launched_(incognito_launched) {}

  bool incognito_launched_;

  DISALLOW_COPY_AND_ASSIGN(TestIncognitoObserver);
};

// Allows access to some private methods for testing.
class TestPerfCollector : public PerfCollector {
 public:
  TestPerfCollector() = default;

  using MetricCollector::CollectPerfDataAfterSessionRestore;
  using MetricCollector::OnJankStarted;
  using MetricCollector::OnJankStopped;
  using MetricCollector::ShouldCollect;
  using MetricCollector::StopTimer;
  using PerfCollector::AddCachedDataDelta;
  using PerfCollector::collection_params;
  using PerfCollector::command_selector;
  using PerfCollector::Init;
  using PerfCollector::IsRunning;
  using PerfCollector::max_frequencies_mhz;
  using PerfCollector::ParseOutputProtoIfValid;
  using PerfCollector::RecordUserLogin;
  using PerfCollector::set_profile_done_callback;

  bool collection_stopped() { return collection_stopped_; }
  bool collection_done() { return !real_callback_; }

 protected:
  std::unique_ptr<PerfOutputCall> CreatePerfOutputCall(
      base::TimeDelta duration,
      const std::vector<std::string>& perf_args,
      PerfOutputCall::DoneCallback callback) override {
    real_callback_ = std::move(callback);

    return std::make_unique<FakePerfOutputCall>(
        duration,
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

  DISALLOW_COPY_AND_ASSIGN(TestPerfCollector);
};

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);
const base::TimeDelta kCollectionDuration = base::TimeDelta::FromSeconds(2);

}  // namespace

class PerfCollectorTest : public testing::Test {
 public:
  PerfCollectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

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

  DISALLOW_COPY_AND_ASSIGN(PerfCollectorTest);
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

TEST_F(PerfCollectorTest, NoCollectionWhenProfileCacheFull) {
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(perf_collector_->IsRunning());
  // Pretend the cache is full.
  perf_collector_->AddCachedDataDelta(4 * 1024 * 1024);

  // Advance the clock by a periodic collection interval. We shouldn't find a
  // profile because the cache is full.
  task_environment_.FastForwardBy(kPeriodicCollectionInterval);
  EXPECT_TRUE(cached_profile_data_.empty());
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

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  // An incognito window opens.
  incognito_observer = TestIncognitoObserver::CreateWithIncognitoLaunched(true);
  perf_collector_->ParseOutputProtoIfValid(std::move(incognito_observer),
                                           std::move(sampled_profile), true,
                                           perf_data_proto.SerializeAsString());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(cached_profile_data_.empty());

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

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_IvyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x3a;  // IvyBridge
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpu(cpuid);
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfRecordCyclesCmd);
  EXPECT_EQ(cmds[1].value, kPerfRecordCallgraphCmd);
  auto found =
      std::find_if(cmds.begin(), cmds.end(),
                   [](const RandomSelector::WeightAndValue& cmd) -> bool {
                     return cmd.value == kPerfRecordLBRCmd;
                   });
  EXPECT_NE(cmds.end(), found);
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordCacheMissesCmd;
                       });
  EXPECT_NE(cmds.end(), found);
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_SandyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2a;  // SandyBridge
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpu(cpuid);
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfRecordCyclesCmd);
  EXPECT_EQ(cmds[1].value, kPerfRecordCallgraphCmd);
  auto found =
      std::find_if(cmds.begin(), cmds.end(),
                   [](const RandomSelector::WeightAndValue& cmd) -> bool {
                     return cmd.value == kPerfRecordLBRCmd;
                   });
  EXPECT_NE(cmds.end(), found);
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordCacheMissesCmd;
                       });
  EXPECT_NE(cmds.end(), found);
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnUarch_Goldmont) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x5c;  // Goldmont
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpu(cpuid);
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfRecordCyclesCmd);
  EXPECT_EQ(cmds[1].value, kPerfRecordCallgraphCmd);
  auto found =
      std::find_if(cmds.begin(), cmds.end(),
                   [](const RandomSelector::WeightAndValue& cmd) -> bool {
                     return cmd.value == kPerfRecordLBRCmdAtom;
                   });
  EXPECT_NE(cmds.end(), found);
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordCacheMissesCmd;
                       });
  EXPECT_NE(cmds.end(), found);
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordDataTLBMissesCmd;
                       });
  EXPECT_EQ(cmds.end(), found) << "Goldmont requires specialized dTLB command";
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordDataTLBMissesCmdGLM;
                       });
  EXPECT_NE(cmds.end(), found);
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_Arm) {
  CPUIdentity cpuid;
  cpuid.arch = "armv7l";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpu(cpuid);
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfRecordCyclesCmd);
  EXPECT_EQ(cmds[1].value, kPerfRecordCallgraphCmd);
  auto found =
      std::find_if(cmds.begin(), cmds.end(),
                   [](const RandomSelector::WeightAndValue& cmd) -> bool {
                     return cmd.value == kPerfRecordLBRCmd;
                   });
  EXPECT_EQ(cmds.end(), found) << "ARM does not support this command";
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordCacheMissesCmd;
                       });
  EXPECT_EQ(cmds.end(), found) << "ARM does not support this command";
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_x86_32) {
  CPUIdentity cpuid;
  cpuid.arch = "x86";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2f;  // Westmere
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpu(cpuid);
  ASSERT_GE(cmds.size(), 2UL);
  EXPECT_EQ(cmds[0].value, kPerfRecordCyclesCmd);
  EXPECT_EQ(cmds[1].value, kPerfRecordCallgraphCmd);
  auto found =
      std::find_if(cmds.begin(), cmds.end(),
                   [](const RandomSelector::WeightAndValue& cmd) -> bool {
                     return cmd.value == kPerfRecordLBRCmd;
                   });
  EXPECT_EQ(cmds.end(), found) << "x86_32 does not support this command";
  found = std::find_if(cmds.begin(), cmds.end(),
                       [](const RandomSelector::WeightAndValue& cmd) -> bool {
                         return cmd.value == kPerfRecordCacheMissesCmd;
                       });
  EXPECT_EQ(cmds.end(), found) << "x86_32 does not support this command";
}

TEST_F(PerfCollectorTest, DefaultCommandsBasedOnArch_Unknown) {
  CPUIdentity cpuid;
  cpuid.arch = "nonsense";
  cpuid.vendor = "";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  std::vector<RandomSelector::WeightAndValue> cmds =
      internal::GetDefaultCommandsForCpu(cpuid);
  EXPECT_EQ(1UL, cmds.size());
  EXPECT_EQ(cmds[0].value, kPerfRecordCyclesCmd);
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

  perf_collector_->CollectPerfDataAfterSessionRestore(
      base::TimeDelta::FromSeconds(1), kRestoredTabs);
  // Timer is active after the OnSessionRestoreDone call.
  EXPECT_TRUE(perf_collector_->IsRunning());
  // A collection in action: should reject another collection request.
  EXPECT_FALSE(perf_collector_->ShouldCollect());

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  // A collection is ongoing. Triggering a jankiness collection should have no
  // effect on the existing collection.
  perf_collector_->OnJankStarted();
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
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

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));

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

class PerfCollectorCollectionParamsTest : public testing::Test {
 public:
  PerfCollectorCollectionParamsTest() {}

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(PerfCollectorCollectionParamsTest);
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
      internal::GetDefaultCommandsForCpu(GetCPUIdentity());
  std::map<std::string, std::string> params;
  ASSERT_TRUE(variations::AssociateVariationParams(
      "ChromeOSWideProfilingCollection", "group_name", params));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "ChromeOSWideProfilingCollection", "group_name"));

  auto perf_collector = std::make_unique<TestPerfCollector>();
  EXPECT_TRUE(perf_collector->command_selector().odds().empty());
  perf_collector->Init();
  EXPECT_EQ(default_cmds, perf_collector->command_selector().odds());
}

TEST_F(PerfCollectorCollectionParamsTest, Commands_InvalidValues) {
  std::vector<RandomSelector::WeightAndValue> default_cmds =
      internal::GetDefaultCommandsForCpu(GetCPUIdentity());
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
  ASSERT_TRUE(variations::AssociateVariationParams(
      "ChromeOSWideProfilingCollection", "group_name", params));
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
      internal::GetDefaultCommandsForCpu(GetCPUIdentity());
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
  ASSERT_TRUE(variations::AssociateVariationParams(
      "ChromeOSWideProfilingCollection", "group_name", params));
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
  ASSERT_TRUE(variations::AssociateVariationParams(
      "ChromeOSWideProfilingCollection", "group_name", params));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "ChromeOSWideProfilingCollection", "group_name"));

  auto perf_collector = std::make_unique<TestPerfCollector>();
  const auto& parsed_params = perf_collector->collection_params();

  // Not initialized yet:
  EXPECT_NE(base::TimeDelta::FromSeconds(15),
            parsed_params.collection_duration);
  EXPECT_NE(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);

  perf_collector->Init();

  EXPECT_EQ(base::TimeDelta::FromSeconds(15),
            parsed_params.collection_duration);
  EXPECT_EQ(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);
}

TEST(PerfCollectorInternalTest, CommandSamplesCPUCycles) {
  EXPECT_TRUE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "cycles", "-c", "1000003"}));
  EXPECT_TRUE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "cycles", "-g", "-c", "4000037"}));
  EXPECT_TRUE(internal::CommandSamplesCPUCycles({"perf", "record", "-a", "-e",
                                                 "cycles", "-c", "4000037",
                                                 "--call-graph", "lbr"}));

  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "r20c4", "-b", "-c", "200011"}));
  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "rc4", "-b", "-c", "300001"}));
  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "r0481", "-c", "2003"}));
  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "r13d0", "-c", "2003"}));
  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "iTLB-misses", "-c", "2003"}));
  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "dTLB-misses", "-c", "2003"}));
  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "record", "-a", "-e", "cache-misses", "-c", "10007"}));

  EXPECT_TRUE(internal::CommandSamplesCPUCycles({"perf", "record", "-a", "-e",
                                                 "instructions", "-e", "cycles",
                                                 "-c", "1000003"}));

  EXPECT_FALSE(internal::CommandSamplesCPUCycles(
      {"perf", "stat", "-a", "-e", "cycles", "-e", "instructions"}));
}

}  // namespace metrics
