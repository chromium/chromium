// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ChromeOSHistogramMetricsProvider, NoCommandLine) {
  base::HistogramTester histogram_tester;

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_OnlyOne) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "1");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_NotInts) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "s");
  command_line->AppendSwitchASCII("feature-management-max-level", "t");
  command_line->AppendSwitchASCII("feature-management-scope", "u");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_OnlyOneInt) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "1");
  command_line->AppendSwitchASCII("feature-management-max-level", "t");
  command_line->AppendSwitchASCII("feature-management-scope", "0");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_Invalid_Level) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "-1");
  command_line->AppendSwitchASCII("feature-management-max-level", "1");
  command_line->AppendSwitchASCII("feature-management-scope", "0");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_Negative_MaxLevel) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "1");
  command_line->AppendSwitchASCII("feature-management-max-level", "-1");
  command_line->AppendSwitchASCII("feature-management-scope", "0");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_Small_MaxLevel) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "1");
  command_line->AppendSwitchASCII("feature-management-max-level", "0");
  command_line->AppendSwitchASCII("feature-management-scope", "0");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_Success_NonCBX) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "0");
  command_line->AppendSwitchASCII("feature-management-max-level", "1");
  command_line->AppendSwitchASCII("feature-management-scope", "0");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_TRUE(provider.ProvideHistograms());
  histogram_tester.ExpectUniqueSample("Platform.Segmentation.FeatureLevel", 0,
                                      1);
  histogram_tester.ExpectUniqueSample("Platform.Segmentation.ScopeLevel", 0, 1);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_Success_HB) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "1");
  command_line->AppendSwitchASCII("feature-management-max-level", "1");
  command_line->AppendSwitchASCII("feature-management-scope", "1");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_TRUE(provider.ProvideHistograms());
  histogram_tester.ExpectUniqueSample("Platform.Segmentation.FeatureLevel", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Platform.Segmentation.ScopeLevel", 2, 1);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_Success_SB) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "1");
  command_line->AppendSwitchASCII("feature-management-max-level", "1");
  command_line->AppendSwitchASCII("feature-management-scope", "0");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_TRUE(provider.ProvideHistograms());
  histogram_tester.ExpectUniqueSample("Platform.Segmentation.FeatureLevel", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Platform.Segmentation.ScopeLevel", 1, 1);
}

TEST(ChromeOSHistogramMetricsProvider, CommandLine_NonCBX_Hardbranded) {
  base::HistogramTester histogram_tester;

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII("feature-management-level", "0");
  command_line->AppendSwitchASCII("feature-management-max-level", "1");
  command_line->AppendSwitchASCII("feature-management-scope", "1");

  ChromeOSHistogramMetricsProvider provider;
  EXPECT_FALSE(provider.ProvideHistograms());
  histogram_tester.ExpectTotalCount("Platform.Segmentation.FeatureLevel", 0);
  histogram_tester.ExpectTotalCount("Platform.Segmentation.ScopeLevel", 0);
}
}  // namespace
