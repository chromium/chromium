// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/pressure/pressure_metrics.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kCPUPressureHistogramName[] = "System.Pressure.CPU";
}

TEST(PressureMetricsTest, ParseInvalidPressureFile) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath cpu_pressure_path = tmp_dir.GetPath().Append("cpu");
  PressureMetrics cpu_pressure(kCPUPressureHistogramName, cpu_pressure_path);

  struct {
    const char* message;
    const char* file_content;
  } kTestCases[] = {
      {"An empty file should not parse", ""},
      {"Single line file should not parse",
       "some avg10=27.12 avg60=20.01 avg300=10.42 total=13351656 full "
       "avg10=7.52 avg60=4.01 avg300=0.42 total=168256"},
      {"Incorrect prefix should not parse",
       "S0M3 avg10=27.12 avg60=20.01 avg300=10.42 total=13351656\n"
       "FU11 avg10=7.52 avg60=4.01 avg300=0.42 total=168256\n"},
      {"Incomplete line should not parse", "some\nfull"},
      {"Missing columns should not parse.",
       "some avg10=27.12\nfull avg10=7.52"},
      {"Invalid float values should not parse.",
       "some avg10=XX.YY avg60=20.01 avg300=10.42 total=13351656\n"
       "full avg10=7.52 avg60=4.01 avg300=0.42 total=168256\n"},
      {"Flipped lines should not parse",
       "full avg10=0.04 avg60=0.05 avg300=0.06 total=1\n"
       "some avg10=0.01 avg60=0.02 avg300=0.03 total=1\n"}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << test_case.message);
    ASSERT_TRUE(base::WriteFile(cpu_pressure_path, test_case.file_content));
    EXPECT_FALSE(cpu_pressure.CollectCurrentPressure().has_value());
  }
}

TEST(PressureMetricsTest, ParseValidPressureFile) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath cpu_pressure_path = tmp_dir.GetPath().Append("cpu");
  PressureMetrics cpu_pressure(kCPUPressureHistogramName, cpu_pressure_path);

  struct {
    const char* message;
    const char* file_content;
  } kTestCases[] = {
      {"No ending newline",
       "some avg10=0.01 avg60=0.02 avg300=0.03 total=1\n"
       "full avg10=0.04 avg60=0.05 avg300=0.06 total=1"},
      {"One ending newline",
       "some avg10=0.01 avg60=0.02 avg300=0.03 total=1\n"
       "full avg10=0.04 avg60=0.05 avg300=0.06 total=1\n"},
      {"Multiple ending newlines",
       "some avg10=0.01 avg60=0.02 avg300=0.03 total=1\n"
       "full avg10=0.04 avg60=0.05 avg300=0.06 total=1\n\n"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << test_case.message);
    ASSERT_TRUE(base::WriteFile(cpu_pressure_path, test_case.file_content));
    EXPECT_TRUE(cpu_pressure.CollectCurrentPressure().has_value());
  }
}

TEST(PressureMetricsTest, ParseValidPressureFileAndPressureValues) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath cpu_pressure_path = tmp_dir.GetPath().Append("cpu");
  PressureMetrics cpu_pressure(kCPUPressureHistogramName, cpu_pressure_path);

  // Valid input should parse.
  ASSERT_TRUE(
      base::WriteFile(cpu_pressure_path,
                      "some avg10=0.01 avg60=0.02 avg300=0.03 total=1\n"
                      "full avg10=0.04 avg60=0.05 avg300=0.06 total=1\n"));
  std::optional<PressureMetrics::Sample> sample =
      cpu_pressure.CollectCurrentPressure();
  EXPECT_TRUE(sample.has_value());

  EXPECT_DOUBLE_EQ(sample->some_avg10, 0.01);
  EXPECT_DOUBLE_EQ(sample->some_avg60, 0.02);
  EXPECT_DOUBLE_EQ(sample->some_avg300, 0.03);
  EXPECT_DOUBLE_EQ(sample->full_avg10, 0.04);
  EXPECT_DOUBLE_EQ(sample->full_avg60, 0.05);
  EXPECT_DOUBLE_EQ(sample->full_avg300, 0.06);
}

TEST(PressureMetricsTest, MetricsAreEmitted) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath cpu_pressure_path = tmp_dir.GetPath().Append("cpu");
  ASSERT_TRUE(base::WriteFile(
      cpu_pressure_path,
      "some avg10=27.12 avg60=20.01 avg300=10.42 total=13351656\nfull "
      "avg10=7.52 avg60=4.01 avg300=0.42 total=168256\n"));

  PressureMetrics cpu_pressure(kCPUPressureHistogramName, cpu_pressure_path);
  std::optional<PressureMetrics::Sample> sample =
      cpu_pressure.CollectCurrentPressure();
  ASSERT_TRUE(sample.has_value());

  base::HistogramTester histogram_tester;
  cpu_pressure.ReportToUMA(sample.value());

  constexpr int kExpectedPressure = 27;
  histogram_tester.ExpectUniqueSample(kCPUPressureHistogramName,
                                      kExpectedPressure, 1);
}
