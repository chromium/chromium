// Copyright (c) 2021 The Chromium Authors. All rights reserved.
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

// Number of decimals not consistent, slightly malformed - but acceptable.
const char kFileContents2[] =
    "some avg10=24 avg60=5.06 avg300=15.10 total=417963\n"
    "full avg10=9.2 avg60=19.20 avg300=3.23 total=205933\n";

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
  const std::string& GetMetricPrefix() { return cit_->metric_prefix_; }
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

TEST_F(PSIMemoryMetricsTest, CustomInterval) {
  Init(60);

  EXPECT_EQ(base::Seconds(60), GetCollection());
}

TEST_F(PSIMemoryMetricsTest, InvalidInterval) {
  Init(15);

  EXPECT_EQ(base::Seconds(10), GetCollection());
}

TEST_F(PSIMemoryMetricsTest, SunnyDay1) {
  Init(10);
  int bytes_written = base::WriteFile(GetTestFileName(), kFileContents1,
                                      sizeof(kFileContents1) - 1);

  EXPECT_EQ(sizeof(kFileContents1) - 1, bytes_written);

  Cit()->CollectEvents();

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 2310 /*bucket*/, 1 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 900 /*bucket*/, 1 /*count*/);
}

TEST_F(PSIMemoryMetricsTest, TestWithTimer) {
  Init(10);
  int bytes_written = base::WriteFile(GetTestFileName(), kFileContents1,
                                      sizeof(kFileContents1) - 1);

  EXPECT_EQ(sizeof(kFileContents1) - 1, bytes_written);

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

  EXPECT_EQ(sizeof(kFileContents1) - 1, bytes_written);

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

  EXPECT_EQ(sizeof(kFileContents1) - 1, bytes_written);

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

  EXPECT_EQ(sizeof(kFileContents1) - 1, bytes_written);

  Cit()->CollectEvents();

  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Some",
                                 1510 /*bucket*/, 1 /*count*/);
  Histograms().ExpectBucketCount("ChromeOS.CWP.PSIMemPressure.Full",
                                 323 /*bucket*/, 1 /*count*/);
}

TEST_F(PSIMemoryMetricsTest, InternalsA) {
  Init(10);

  std::string testContent1 = "prefix" + GetMetricPrefix() + "9.37 suffix";
  EXPECT_EQ(base::Seconds(10), GetCollection());

  size_t s = 0;
  size_t e = 0;

  EXPECT_EQ(false, internal::FindMiddleString(testContent1, 0, "nothere",
                                              "suffix", &s, &e));

  EXPECT_EQ(false, internal::FindMiddleString(testContent1, 0, "prefix",
                                              "notthere", &s, &e));

  EXPECT_EQ(true, internal::FindMiddleString(testContent1, 0, "prefix",
                                             "suffix", &s, &e));
  EXPECT_EQ(6, s);
  EXPECT_EQ(17, e);

  EXPECT_EQ(937, Cit()->GetMetricValue(testContent1, s, e));

  std::string testContent2 = "extra " + testContent1;
  EXPECT_EQ(true, internal::FindMiddleString(testContent2, 0, "prefix",
                                             "suffix", &s, &e));
  EXPECT_EQ(12, s);
  EXPECT_EQ(23, e);

  EXPECT_EQ(937, Cit()->GetMetricValue(testContent2, s, e));
}

TEST_F(PSIMemoryMetricsTest, InternalsB) {
  Init(300);

  int msome;
  int mfull;
  PSIMemoryMetrics::ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents1, &msome, &mfull);

  EXPECT_EQ(PSIMemoryMetrics::ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(1510, msome);
  EXPECT_EQ(323, mfull);
}

TEST_F(PSIMemoryMetricsTest, InternalsC) {
  Init(60);

  int msome;
  int mfull;
  PSIMemoryMetrics::ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents1, &msome, &mfull);

  EXPECT_EQ(PSIMemoryMetrics::ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(506, msome);
  EXPECT_EQ(1920, mfull);
}

TEST_F(PSIMemoryMetricsTest, InternalsD) {
  Init(10);

  int msome;
  int mfull;
  PSIMemoryMetrics::ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents1, &msome, &mfull);

  EXPECT_EQ(PSIMemoryMetrics::ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(2310, msome);
  EXPECT_EQ(900, mfull);
}

TEST_F(PSIMemoryMetricsTest, InternalsE) {
  Init(10);

  int msome;
  int mfull;
  PSIMemoryMetrics::ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents2, &msome, &mfull);

  EXPECT_EQ(PSIMemoryMetrics::ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(2400, msome);
  EXPECT_EQ(920, mfull);
}

}  // namespace ash
