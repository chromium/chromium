// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

class TestObserver : public AlsReader::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override = default;

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(const int lux) override {
    ambient_light_ = lux;
    ++num_received_ambient_lights_;
  }

  void OnAlsReaderInitialized(const AlsReader::AlsInitStatus status) override {
    status_ = base::Optional<AlsReader::AlsInitStatus>(status);
  }

  int ambient_light() { return ambient_light_; }
  int num_received_ambient_lights() { return num_received_ambient_lights_; }

  AlsReader::AlsInitStatus status() {
    CHECK(status_);
    return status_.value();
  }

 private:
  int ambient_light_ = -1;
  int num_received_ambient_lights_ = 0;
  base::Optional<AlsReader::AlsInitStatus> status_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};
}  // namespace

class AlsReaderImplTest : public testing::Test {
 public:
  AlsReaderImplTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    ambient_light_path_ = temp_dir_.GetPath().Append("test_als");
    als_reader_.SetTaskRunnerForTesting(base::SequencedTaskRunnerHandle::Get());
    als_reader_.AddObserver(&test_observer_);
    als_reader_.InitForTesting(ambient_light_path_);
  }

  ~AlsReaderImplTest() override = default;

 protected:
  void WriteLux(int lux) {
    const std::string lux_string = base::NumberToString(lux);
    const int bytes_written = base::WriteFile(
        ambient_light_path_, lux_string.data(), lux_string.size());
    ASSERT_EQ(bytes_written, static_cast<int>(lux_string.size()))
        << "Wrote " << bytes_written << " byte(s) instead of "
        << lux_string.size() << " to " << ambient_light_path_.value();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath ambient_light_path_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  AlsReaderImpl als_reader_;
  TestObserver test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AlsReaderImplTest);
};

TEST_F(AlsReaderImplTest, OnAlsReaderInitialized) {
  EXPECT_EQ(AlsReader::AlsInitStatus::kSuccess, test_observer_.status());
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.AlsReaderStatus",
      static_cast<int>(AlsReader::AlsInitStatus::kSuccess), 1);
}

TEST_F(AlsReaderImplTest, ErrorMetrics) {
  std::string histogram = "AutoScreenBrightness.AlsReaderStatus";

  // The setup will already have logged a success during setup.
  // Double-check that's the case.
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.AlsReaderStatus",
      static_cast<int>(AlsReader::AlsInitStatus::kSuccess), 1);

  als_reader_.FailForTesting();
  histogram_tester_.ExpectBucketCount(
      histogram, static_cast<int>(AlsReader::AlsInitStatus::kDisabled), 1);
  histogram_tester_.ExpectBucketCount(
      histogram, static_cast<int>(AlsReader::AlsInitStatus::kMissingPath), 1);
  // Expect 2 errors from above + 1 success from before |FailForTesting|.
  histogram_tester_.ExpectTotalCount(histogram, 3);
}

TEST_F(AlsReaderImplTest, OneAlsValue) {
  WriteLux(10);
  task_environment_->RunUntilIdle();
  EXPECT_EQ(10, test_observer_.ambient_light());
  EXPECT_EQ(1, test_observer_.num_received_ambient_lights());
}

TEST_F(AlsReaderImplTest, TwoAlsValues) {
  WriteLux(10);
  // Ambient light is read immediately after initialization, and then
  // periodically every |kAlsPollInterval|. Below we move time for half of
  // |kAlsPollInterval| to ensure there is only one reading attempt.
  task_environment_->FastForwardBy(AlsReaderImpl::kAlsPollInterval / 2);
  EXPECT_EQ(10, test_observer_.ambient_light());
  EXPECT_EQ(1, test_observer_.num_received_ambient_lights());

  WriteLux(20);
  // Now move time for another |kAlsPollInterval| to trigger another read.
  task_environment_->FastForwardBy(AlsReaderImpl::kAlsPollInterval);
  EXPECT_EQ(20, test_observer_.ambient_light());
  EXPECT_EQ(2, test_observer_.num_received_ambient_lights());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
