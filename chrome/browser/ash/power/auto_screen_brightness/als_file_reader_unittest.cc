// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/als_file_reader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

class AlsFileReaderTest : public testing::Test {
 public:
  AlsFileReaderTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    als_file_reader_ = std::make_unique<AlsFileReader>(&als_reader_);

    CHECK(temp_dir_.CreateUniqueTempDir());
    ambient_light_path_ = temp_dir_.GetPath().Append("test_als");
    als_file_reader_->SetTaskRunnerForTesting(
        base::SequencedTaskRunner::GetCurrentDefault());
    als_reader_.AddObserver(&fake_observer_);
    als_file_reader_->InitForTesting(ambient_light_path_);
  }

  AlsFileReaderTest(const AlsFileReaderTest&) = delete;
  AlsFileReaderTest& operator=(const AlsFileReaderTest&) = delete;
  ~AlsFileReaderTest() override = default;

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

  std::unique_ptr<AlsFileReader> als_file_reader_;
  AlsReader als_reader_;
  FakeObserver fake_observer_;
};

TEST_F(AlsFileReaderTest, OnAlsReaderInitialized) {
  EXPECT_EQ(AlsReader::AlsInitStatus::kSuccess, fake_observer_.status());
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.AlsReaderStatus",
      static_cast<int>(AlsReader::AlsInitStatus::kSuccess), 1);
}

TEST_F(AlsFileReaderTest, ErrorMetrics) {
  std::string histogram = "AutoScreenBrightness.AlsReaderStatus";

  // The setup will already have logged a success during setup.
  // Double-check that's the case.
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.AlsReaderStatus",
      static_cast<int>(AlsReader::AlsInitStatus::kSuccess), 1);

  als_file_reader_->FailForTesting();
  histogram_tester_.ExpectBucketCount(
      histogram, static_cast<int>(AlsReader::AlsInitStatus::kDisabled), 1);
  histogram_tester_.ExpectBucketCount(
      histogram, static_cast<int>(AlsReader::AlsInitStatus::kMissingPath), 1);
  // Expect 2 errors from above + 1 success from before |FailForTesting|.
  histogram_tester_.ExpectTotalCount(histogram, 3);
}

TEST_F(AlsFileReaderTest, OneAlsValue) {
  WriteLux(10);
  task_environment_->RunUntilIdle();
  EXPECT_EQ(10, fake_observer_.ambient_light());
  EXPECT_EQ(1, fake_observer_.num_received_ambient_lights());
}

TEST_F(AlsFileReaderTest, TwoAlsValues) {
  WriteLux(10);
  // Ambient light is read immediately after initialization, and then
  // periodically every |kAlsPollInterval|. Below we move time for half of
  // |kAlsPollInterval| to ensure there is only one reading attempt.
  task_environment_->FastForwardBy(AlsFileReader::kAlsPollInterval / 2);
  EXPECT_EQ(10, fake_observer_.ambient_light());
  EXPECT_EQ(1, fake_observer_.num_received_ambient_lights());

  WriteLux(20);
  // Now move time for another |kAlsPollInterval| to trigger another read.
  task_environment_->FastForwardBy(AlsFileReader::kAlsPollInterval);
  EXPECT_EQ(20, fake_observer_.ambient_light());
  EXPECT_EQ(2, fake_observer_.num_received_ambient_lights());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
