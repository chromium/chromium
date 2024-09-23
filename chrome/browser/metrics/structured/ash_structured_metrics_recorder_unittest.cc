// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"

#include <cstdint>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/metrics/structured/ash_event_storage.h"
#include "chrome/browser/metrics/structured/key_data_provider_ash.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {

namespace {

// These project, event, and metric names are used for testing.

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
// The name hash of "TestProjectTwo".
constexpr uint64_t kProjectTwoHash = UINT64_C(5876808001962504629);
// The name hash of "TestProjectThree".
constexpr uint64_t kProjectThreeHash = UINT64_C(10860358748803291132);
// The name hash of "TestProjectFour".
constexpr uint64_t kProjectFourHash = UINT64_C(6801665881746546626);
// The name hash of "CrOSEvents"
constexpr uint64_t kCrOSEventsProjectHash = UINT64_C(12657197978410187837);
// The name has for "SequencedTestProject"
constexpr uint64_t kSequencedTestProjectHash = UINT64_C(7434962983641669694);

// The name hash of "chrome::TestProjectOne::TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);
// The name hash of "chrome::TestProjectTwo::TestEventTwo".
constexpr uint64_t kEventTwoHash = UINT64_C(8995967733561999410);
// The name hash of "chrome::TestProjectFour::TestEventFive".
constexpr uint64_t kEventFiveHash = UINT64_C(7045523601811399253);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);
// The name hash of "TestMetricThree".
constexpr uint64_t kMetricThreeHash = UINT64_C(13469300759843809564);
// The name hash of "TestMetricFive".
constexpr uint64_t kMetricFiveHash = UINT64_C(8665976921794972190);

// The hex-encoded first 8 bytes of SHA256("aaa...a")
constexpr char kProjectOneId[] = "3BA3F5F43B926026";
// The hex-encoded first 8 bytes of SHA256("bbb...b")
constexpr char kProjectTwoId[] = "BDB339768BC5E4FE";
// The hex-encoded first 8 bytes of SHA256("ddd...d")
constexpr char kProjectFourId[] = "FBBBB6DE2AA74C3C";

constexpr char kHwid[] = "hwid";
constexpr size_t kUserCount = 3;

// Test values.
constexpr char kValueOne[] = "value one";
constexpr char kValueTwo[] = "value two";

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

// Make a simple testing proto with one |uma_events| message for each id in
// |ids|.
EventsProto MakeExternalEventProto(const std::vector<uint64_t>& ids) {
  EventsProto proto;

  for (const auto id : ids) {
    auto* event = proto.add_events();
    event->set_profile_event_id(id);
  }

  return proto;
}

class TestRecorder : public StructuredMetricsClient::RecordingDelegate {
 public:
  TestRecorder() = default;
  TestRecorder(const TestRecorder& recorder) = delete;
  TestRecorder& operator=(const TestRecorder& recorder) = delete;
  ~TestRecorder() override = default;

  void RecordEvent(Event&& event) override {
    Recorder::GetInstance()->RecordEvent(std::move(event));
  }

  bool IsReadyToRecord() const override { return true; }
};

class TestSystemProfileProvider : public metrics::MetricsProvider {
 public:
  TestSystemProfileProvider() = default;
  TestSystemProfileProvider(const TestSystemProfileProvider& recorder) = delete;
  TestSystemProfileProvider& operator=(
      const TestSystemProfileProvider& recorder) = delete;
  ~TestSystemProfileProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* proto) override {
    proto->set_multi_profile_user_count(kUserCount);
    proto->mutable_hardware()->set_full_hardware_class(kHwid);
  }
};

}  // namespace

class AshStructuredMetricsRecorderTest : public testing::Test {
 public:
  AshStructuredMetricsRecorderTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  AshStructuredMetricsRecorderTest(const AshStructuredMetricsRecorderTest&) =
      delete;
  AshStructuredMetricsRecorderTest& operator=(
      const AshStructuredMetricsRecorderTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp(temp_dir_.GetPath()));

    // Fixed paths to store keys for test.
    device_key_path_ =
        temp_dir_.GetPath().Append("structured_metrics").Append("device_keys");
    profile_key_path_ =
        GetProfilePath().Append("structured_metrics").Append("keys");

    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    StructuredMetricsClient::Get()->SetDelegate(&test_recorder_);
    // Move the mock date forward from day 0, because KeyData assumes that day 0
    // is a bug.
    task_environment_.AdvanceClock(base::Days(1000));
  }

  void TearDown() override {
    StructuredMetricsClient::Get()->UnsetDelegate();
    profile_manager_.DeleteAllTestingProfiles();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void WriteTestingProfileKeys() {
    const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

    KeyDataProto proto;
    KeyProto& key_one = (*proto.mutable_keys())[kProjectOneHash];
    key_one.set_key("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    key_one.set_last_rotation(today);
    key_one.set_rotation_period(90);

    KeyProto& key_two = (*proto.mutable_keys())[kProjectTwoHash];
    key_two.set_key("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    key_two.set_last_rotation(today);
    key_two.set_rotation_period(90);

    KeyProto& key_three = (*proto.mutable_keys())[kProjectThreeHash];
    key_three.set_key("cccccccccccccccccccccccccccccccc");
    key_three.set_last_rotation(today);
    key_three.set_rotation_period(90);

    KeyProto& cros_events = (*proto.mutable_keys())[kCrOSEventsProjectHash];
    cros_events.set_key("cccccccccccccccccccccccccccccccc");
    cros_events.set_last_rotation(today);
    cros_events.set_rotation_period(90);

    base::CreateDirectory(ProfileKeyFilePath().DirName());
    ASSERT_TRUE(
        base::WriteFile(ProfileKeyFilePath(), proto.SerializeAsString()));
    Wait();
  }

  base::FilePath TempDirPath() { return temp_dir_.GetPath(); }

  base::FilePath ProfileKeyFilePath() { return profile_key_path_; }

  base::FilePath DeviceKeyFilePath() { return device_key_path_; }

  base::FilePath GetProfilePath() {
    // u-p1@test-hash is the directory name generated for user p1. This name
    // seems to be consistent. If it changes, update the directory name. It is
    // done this way so the directory can be pre-populated with key data such
    // that it can be used in the remaining tests.
    return profile_manager_.profiles_dir().Append("u-p1@test-hash");
  }

  base::FilePath PreLoginEventPath() {
    return TempDirPath()
        .Append(FILE_PATH_LITERAL("structured_metrics"))
        .Append(FILE_PATH_LITERAL("device"));
  }

  void OnRecordingEnabled() { recorder_->EnableRecording(); }

  void OnRecordingDisabled() { recorder_->DisableRecording(); }

  void WriteTestingDeviceKeys() {
    const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

    KeyDataProto proto;
    KeyProto& key = (*proto.mutable_keys())[kProjectFourHash];
    key.set_key("dddddddddddddddddddddddddddddddd");
    key.set_last_rotation(today);
    key.set_rotation_period(90);

    base::CreateDirectory(DeviceKeyFilePath().DirName());
    ASSERT_TRUE(
        base::WriteFile(DeviceKeyFilePath(), proto.SerializeAsString()));
    Wait();
  }

  ChromeUserMetricsExtension GetUmaProto() {
    ChromeUserMetricsExtension uma_proto;
    recorder_->ProvideEventMetrics(uma_proto);
    recorder_->ProvideLogMetadata(uma_proto);
    Wait();
    return uma_proto;
  }

  StructuredDataProto GetEventMetrics() {
    return GetUmaProto().structured_data();
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

  void Init() {
    // Create a system profile, normally done by ChromeMetricsServiceClient.
    system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
    recorder_ = base::WrapRefCounted(new AshStructuredMetricsRecorder(
        std::make_unique<KeyDataProviderAsh>(DeviceKeyFilePath(),
                                             base::Seconds(0)),
        std::make_unique<AshEventStorage>(base::Seconds(0),
                                          PreLoginEventPath()),
        system_profile_provider_.get()));

    profile_manager_.CreateTestingProfile("p1");
    OnRecordingEnabled();
  }

  void InitializeSystemProfile() { recorder_->OnSystemProfileInitialized(); }

  void SetExternalMetricsDirForTest(const base::FilePath dir) {
    recorder_->SetExternalMetricsDirForTest(dir);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestSystemProfileProvider> system_profile_provider_;
  scoped_refptr<AshStructuredMetricsRecorder> recorder_;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
  raw_ptr<TestingProfile> test_profile_;

 private:
  TestRecorder test_recorder_;

  TestingProfileManager profile_manager_;
  base::FilePath device_key_path_;
  base::FilePath profile_key_path_;
};

TEST_F(AshStructuredMetricsRecorderTest, EventMetricsProvideSystemProfile) {
  WriteTestingDeviceKeys();
  WriteTestingProfileKeys();
  Init();
  InitializeSystemProfile();

  Wait();

  StructuredMetricsClient::Record(
      std::move(events::v2::test_project_one::TestEventOne()
                    .SetTestMetricOne(kValueOne)
                    .SetTestMetricTwo(12345)));
  StructuredMetricsClient::Record(
      std::move(events::v2::test_project_two::TestEventTwo().SetTestMetricThree(
          kValueTwo)));

  const auto uma_proto = GetUmaProto();
  CHECK(uma_proto.has_system_profile());

  {
    const auto structured_profile = uma_proto.system_profile();
    EXPECT_EQ(structured_profile.multi_profile_user_count(), kUserCount);
    EXPECT_EQ(structured_profile.hardware().full_hardware_class(), kHwid);
  }

  const auto data = uma_proto.structured_data();
  ASSERT_EQ(data.events_size(), 2);

  {  // First event
    const auto& event = data.events(0);
    EXPECT_EQ(event.event_name_hash(), kEventOneHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectOneId);
    ASSERT_EQ(event.metrics_size(), 2);

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricOneHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("aaa...a", concat(hex(kMetricOneHash),
                // kValueOne))
                "8C2469269D142715");
    }

    {  // Second metric
      const auto& metric = event.metrics(1);
      EXPECT_EQ(metric.name_hash(), kMetricTwoHash);
      EXPECT_EQ(metric.value_int64(), 12345);
    }
  }

  {  // Second event
    const auto& event = data.events(1);
    EXPECT_EQ(event.event_name_hash(), kEventTwoHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectTwoId);
    ASSERT_EQ(event.metrics_size(), 1);

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricThreeHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("bbb...b", concat(hex(kProjectTwoHash),
                // kValueTwo))
                "86F0169868588DC7");
    }
  }

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

TEST_F(AshStructuredMetricsRecorderTest,
       DeviceKeysUsedForDeviceScopedProjects) {
  WriteTestingProfileKeys();
  WriteTestingDeviceKeys();
  Init();

  Wait();

  // This event's project has device scope set, so should use the per-device
  // keys set by WriteTestingDeviceKeys. In this case the expected key is
  // "ddd...d", which we observe by checking the ID and HMAC have the correct
  // value given that key.
  StructuredMetricsClient::Record(std::move(
      events::v2::test_project_four::TestEventFive().SetTestMetricFive(
          "value")));

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);

  const auto& event = data.events(0);
  EXPECT_EQ(event.event_name_hash(), kEventFiveHash);
  EXPECT_EQ(event.project_name_hash(), kProjectFourHash);
  // The hex-encoded first 8 bytes of SHA256("ddd...d").
  EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectFourId);
  ASSERT_EQ(event.metrics_size(), 1);

  const auto& metric = event.metrics(0);
  EXPECT_EQ(metric.name_hash(), kMetricFiveHash);
  EXPECT_EQ(HashToHex(metric.value_hmac()),
            // Value of HMAC_256("ddd...d", concat(hex(kMetricFiveHash),
            // "value"))
            "4CC202FAA78FDC7A");

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

TEST_F(AshStructuredMetricsRecorderTest, ExternalMetricsAreReported) {
  const base::FilePath events_dir(TempDirPath().Append("events"));
  base::CreateDirectory(events_dir);

  const auto proto = MakeExternalEventProto({111, 222, 333});
  ASSERT_TRUE(
      base::WriteFile(events_dir.Append("event"), proto.SerializeAsString()));

  Init();
  SetExternalMetricsDirForTest(events_dir);
  OnRecordingEnabled();
  task_environment_.AdvanceClock(base::Hours(10));
  Wait();
  EXPECT_EQ(GetEventMetrics().events_size(), 3);
}

TEST_F(AshStructuredMetricsRecorderTest,
       ExternalMetricsDroppedWhenRecordingDisabled) {
  const base::FilePath events_dir(TempDirPath().Append("events"));
  base::CreateDirectory(events_dir);

  const auto proto = MakeExternalEventProto({111, 222, 333});
  ASSERT_TRUE(
      base::WriteFile(events_dir.Append("event"), proto.SerializeAsString()));

  Init();
  SetExternalMetricsDirForTest(events_dir);
  OnRecordingDisabled();
  task_environment_.AdvanceClock(base::Hours(10));
  Wait();
  EXPECT_EQ(GetEventMetrics().events_size(), 0);
}

// Ensures that events part of event sequence are recorded properly.
TEST_F(AshStructuredMetricsRecorderTest, EventSequenceLogging) {
  Init();

  Wait();

  const int test_time = 50;
  const int test_time2 = 60;
  const double test_metric = 1.0;
  const double test_metric2 = 2.0;

  events::v2::cr_os_events::Test1 test_event;
  EXPECT_TRUE(test_event.IsEventSequenceType());
  test_event.SetEventSequenceMetadata(Event::EventSequenceMetadata(1));
  test_event.SetRecordedTimeSinceBoot(base::Milliseconds(test_time));

  events::v2::sequenced_test_project::Test1 test_event2;
  EXPECT_TRUE(test_event2.IsEventSequenceType());
  test_event2.SetEventSequenceMetadata(Event::EventSequenceMetadata(2));
  test_event2.SetRecordedTimeSinceBoot(base::Milliseconds(test_time2));

  StructuredMetricsClient::Record(
      std::move(test_event.SetMetric1(test_metric)));
  StructuredMetricsClient::Record(
      std::move(test_event2.SetMetric1(test_metric2)));

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 2);
  {
    const auto& event = data.events(0);
    EXPECT_EQ(event.project_name_hash(), kCrOSEventsProjectHash);

    // Sequence events should have both a device and user project id.
    EXPECT_TRUE(event.has_device_project_id());
    EXPECT_TRUE(event.has_user_project_id());

    // Verify that event sequence metadata has been serialized correctly.
    const auto& event_metadata = event.event_sequence_metadata();
    EXPECT_EQ(event_metadata.reset_counter(), 1);
    EXPECT_TRUE(event_metadata.has_event_unique_id());
    EXPECT_EQ(event_metadata.system_uptime(), test_time);

    ASSERT_EQ(event.metrics_size(), 1);
    const auto& metric = event.metrics(0);
    EXPECT_EQ(metric.value_double(), test_metric);
  }
  {
    const auto& event = data.events(1);
    EXPECT_EQ(event.project_name_hash(), kSequencedTestProjectHash);

    // Sequence events should have both a device and user project id.
    EXPECT_TRUE(event.has_device_project_id());
    EXPECT_TRUE(event.has_user_project_id());

    // Verify that event sequence metadata has been serialized correctly.
    const auto& event_metadata = event.event_sequence_metadata();
    EXPECT_EQ(event_metadata.reset_counter(), 2);
    EXPECT_TRUE(event_metadata.has_event_unique_id());
    EXPECT_EQ(event_metadata.system_uptime(), test_time2);

    ASSERT_EQ(event.metrics_size(), 1);
    const auto& metric = event.metrics(0);
    EXPECT_EQ(metric.value_double(), test_metric2);
  }

  ExpectNoErrors();
}

TEST_F(AshStructuredMetricsRecorderTest, CorrectClientAge) {
  WriteTestingProfileKeys();

  Init();

  Wait();

  const int advance_days = 30;
  const uint32_t expected_client_age_weeks = advance_days / 7;

  // Advance clock by 30 days.
  task_environment_.AdvanceClock(base::Days(advance_days));

  events::v2::cr_os_events::NoMetricsEvent test_event;
  test_event.SetEventSequenceMetadata(Event::EventSequenceMetadata(1));
  StructuredMetricsClient::Record(std::move(test_event));

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);
  ASSERT_EQ(data.events(0).event_sequence_metadata().client_id_rotation_weeks(),
            expected_client_age_weeks);
}

}  // namespace metrics::structured
