// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/key_data_prefs_delegate.h"
#include "components/metrics/structured/lib/key_util.h"
#include "components/metrics/structured/lib/proto/key.pb.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_prefs.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {

namespace {

// These project, event, and metric names are used for testing.

// The name hash of "TestProjectFour".
constexpr uint64_t kProjectFourHash = 6801665881746546626ull;

// The name hash of "chrome::TestProjectFour::TestEventFive".
constexpr uint64_t kEventFiveHash = 7045523601811399253ull;

// The name hash of "TestMetricFive".
constexpr uint64_t kMetricFiveHash = 8665976921794972190ull;

// The hex-encoded first 8 bytes of SHA256("ddd...d")
constexpr char kProjectFourId[] = "FBBBB6DE2AA74C3C";

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
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

}  // namespace

class ChromeStructuredMetricsRecorderTest : public testing::Test {
 public:
  void SetUp() override {
    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    StructuredMetricsClient::Get()->SetDelegate(&test_recorder_);
    // Move the mock date forward from day 0, because KeyData assumes that day 0
    // is a bug.
    task_environment_.AdvanceClock(base::Days(1000));

    ChromeStructuredMetricsRecorder::RegisterLocalState(prefs_.registry());
  }

  void TearDown() override { StructuredMetricsClient::Get()->UnsetDelegate(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  void RecordingEnabled() { recorder_->EnableRecording(); }

  void CreateTestingDeviceKeys() {
    const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

    KeyProto key;
    key.set_key("dddddddddddddddddddddddddddddddd");
    key.set_last_rotation(today);
    key.set_rotation_period(90);

    base::Value::Dict dict =
        prefs_.GetDict(prefs::kDeviceKeyDataPrefName).Clone();
    const validator::Validators* validators = validator::Validators::Get();
    auto project_name = validators->GetProjectName(kProjectFourHash);

    dict.Set(*project_name, util::CreateValueFromKeyProto(key));

    prefs_.SetDict(prefs::kDeviceKeyDataPrefName, std::move(dict));
    Wait();
  }

  ChromeUserMetricsExtension GetUmaProto() {
    ChromeUserMetricsExtension uma_proto;
    recorder_->ProvideEventMetrics(uma_proto);
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

  void CreateAndEnableRecorder() {
    recorder_ = base::MakeRefCounted<ChromeStructuredMetricsRecorder>(&prefs_);
    RecordingEnabled();
    ExpectNoErrors();
  }

 protected:
  scoped_refptr<ChromeStructuredMetricsRecorder> recorder_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir temp_dir_;

 private:
  TestRecorder test_recorder_;

  base::FilePath storage_path_;
};

TEST_F(ChromeStructuredMetricsRecorderTest, DeviceEventsRecorded) {
  CreateTestingDeviceKeys();
  CreateAndEnableRecorder();

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

  ExpectNoErrors();
}

}  // namespace metrics::structured
