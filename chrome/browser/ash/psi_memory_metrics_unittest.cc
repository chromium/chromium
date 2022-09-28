// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/psi_memory_metrics.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Just as the kernel outputs.
const char kFileContents1[] =
    "some avg10=23.10 avg60=5.06 avg300=15.10 total=417963\n"
    "full avg10=9.00 avg60=19.20 avg300=3.23 total=205933\n";

}  // namespace

class PSIMemoryMetricsTest : public testing::Test {
 public:
  PSIMemoryMetricsTest() = default;
  ~PSIMemoryMetricsTest() override = default;

  void Init(uint32_t period) {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    testfilename_ = dir_.GetPath().Append("testpsimem.txt");
    cit_ = PSIMemoryMetrics::CreateForTesting(period, testfilename_.value());
  }

  base::TimeDelta GetCollection() { return cit_->collection_interval_; }
  const base::FilePath& GetTestFileName() { return testfilename_; }
  base::HistogramTester& Histograms() { return histogram_tester_; }
  scoped_refptr<PSIMemoryMetrics> Cit() { return cit_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  void KillCit() { cit_.reset(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir dir_;
  scoped_refptr<PSIMemoryMetrics> cit_;
  base::FilePath testfilename_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PSIMemoryMetricsTest, SunnyDay1) {
  Init(10);

  ASSERT_TRUE(base::WriteFile(GetTestFileName(),
                              {kFileContents1, sizeof(kFileContents1)}));

  Cit()->CollectEvents();

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 1 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 1 /*count*/);
}

TEST_F(PSIMemoryMetricsTest, TestWithTimer) {
  Init(10);

  ASSERT_TRUE(base::WriteFile(GetTestFileName(), kFileContents1));

  //  Repeating timer comes on.
  Cit()->Start();

  task_environment().FastForwardBy(base::Seconds(5));
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 0 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 0 /*count*/);

  task_environment().FastForwardBy(base::Seconds(10));

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 1 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 1 /*count*/);

  task_environment().FastForwardBy(base::Seconds(10));

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 2 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 2 /*count*/);

  // No more.
  Cit()->Stop();
  KillCit();

  task_environment().FastForwardBy(base::Seconds(50));

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 2 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 2 /*count*/);
}

TEST_F(PSIMemoryMetricsTest, CancelBeforeFirstRun) {
  Init(300);
  int bytes_written = base::WriteFile(GetTestFileName(), kFileContents1,
                                      sizeof(kFileContents1) - 1);

  EXPECT_EQ(static_cast<int>(sizeof(kFileContents1) - 1), bytes_written);

  //  Repeating timer comes on - but we will cancel before first iteration.
  Cit()->Start();

  task_environment().FastForwardBy(base::Seconds(5));
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 0 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 0 /*count*/);

  // No more.
  Cit()->Stop();
  KillCit();

  task_environment().FastForwardBy(base::Seconds(50));

  task_environment().FastForwardBy(base::Seconds(5));
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 0 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 0 /*count*/);
}

TEST_F(PSIMemoryMetricsTest, SunnyDay2) {
  Init(60);
  int bytes_written = base::WriteFile(GetTestFileName(), kFileContents1,
                                      sizeof(kFileContents1) - 1);

  EXPECT_EQ(static_cast<int>(sizeof(kFileContents1) - 1), bytes_written);

  Cit()->CollectEvents();

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 506 /*bucket*/, 1 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 1920 /*bucket*/, 1 /*count*/);
}

TEST_F(PSIMemoryMetricsTest, SunnyDay3) {
  Init(300);
  int bytes_written = base::WriteFile(GetTestFileName(), kFileContents1,
                                      sizeof(kFileContents1) - 1);

  EXPECT_EQ(static_cast<int>(sizeof(kFileContents1) - 1), bytes_written);

  Cit()->CollectEvents();

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 1510 /*bucket*/, 1 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 323 /*bucket*/, 1 /*count*/);
}

}  // namespace ash
