// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

#include <memory>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyOf;
using testing::HasSubstr;
using testing::Not;

TEST(OptimizationGuideNavigationDataTest, RecordMetricsNoDataNoCommit) {
  base::test::TaskEnvironment env;

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure no UMA recorded.
  EXPECT_THAT(histogram_tester.GetAllHistogramsRecorded(),
              Not(AnyOf(HasSubstr("OptimizationGuide.ApplyDecision"),
                        HasSubstr("OptimizationGuide.HintCache"),
                        HasSubstr("OptimizationGuide.Hints."),
                        HasSubstr("OptimizationGuide.TargetDecision"))));

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsNoDataHasCommit) {
  base::test::TaskEnvironment env;

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.RecordMetrics(/*has_committed=*/true);

  // Make sure no UMA recorded.
  EXPECT_THAT(histogram_tester.GetAllHistogramsRecorded(),
              Not(AnyOf(HasSubstr("OptimizationGuide.Hints."),
                        HasSubstr("OptimizationGuide.HintCache"))));
  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsCoveredByFetchButNoHintLoadAttempted) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_was_host_covered_by_fetch_at_navigation_start(true);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HasHint.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", 0);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheNoHostMatchBeforeCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_before_commit(false);
  data.set_was_host_covered_by_fetch_at_navigation_start(true);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HasHint.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", 0);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheNoHostMatchBeforeCommitAlsoNotCoveredByFetch) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_before_commit(false);
  data.set_was_host_covered_by_fetch_at_navigation_start(false);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HasHint.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", 0);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheNoHintButCoveredByFetchAtCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_before_commit(false);
  data.set_has_hint_after_commit(false);
  data.set_was_host_covered_by_fetch_at_navigation_start(false);
  data.set_was_host_covered_by_fetch_at_commit(true);
  data.RecordMetrics(/*has_committed=*/true);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.AtCommit", false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", 0);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheNoHintAtCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_after_commit(false);
  data.RecordMetrics(/*has_committed=*/true);

  // This probably wouldn't actually happen in practice but make sure optional
  // check works for before commit.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.AtCommit", false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", 0);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheHasHintButNotLoadedAtCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_after_commit(true);
  data.RecordMetrics(/*has_committed=*/true);

  // This probably wouldn't actually happen in practice but make sure optional
  // check works for before commit.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", 0);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheHasPageHintAtCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_before_commit(true);
  data.set_was_host_covered_by_fetch_at_navigation_start(false);
  data.set_has_hint_after_commit(true);
  data.set_serialized_hint_version_string("abc");
  data.set_page_hint(std::make_unique<optimization_guide::proto::PageHint>());
  data.RecordMetrics(/*has_committed=*/true);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", true, 1);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheHasHintButPageHintNotSetAtCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_before_commit(true);
  data.set_has_hint_after_commit(true);
  data.set_serialized_hint_version_string("abc");
  data.RecordMetrics(/*has_committed=*/true);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", false, 1);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCacheHasHintButNoPageHintAtCommit) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_has_hint_before_commit(true);
  data.set_has_hint_after_commit(true);
  data.set_serialized_hint_version_string("abc");
  data.set_page_hint(nullptr);
  data.RecordMetrics(/*has_committed=*/true);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.BeforeCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.BeforeCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.Hints.NavigationHostCoverage.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HasHint.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.HostMatch.AtCommit", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintCache.PageMatch.AtCommit", false, 1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsBadHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_serialized_hint_version_string("123");
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsEmptyHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/123);
  data.set_serialized_hint_version_string("");
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsZeroTimestampOrSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(0);
  hint_version.set_hint_source(optimization_guide::proto::HINT_SOURCE_UNKNOWN);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsGoodHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithUnknownSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  hint_version.set_hint_source(optimization_guide::proto::HINT_SOURCE_UNKNOWN);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure version is serialized properly and UKM is only recorded for
  // non-empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithNoSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithZeroTimestamp) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(0);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure version is serialized properly and UKM is only recorded for
  // non-empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithNoTimestamp) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsOptimizationTargetModelVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetModelVersionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 2);
  data.RecordMetrics(/*has_committed=*/false);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kPainfulPageLoadModelVersionName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kPainfulPageLoadModelVersionName,
      2);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsModelVersionForOptimizationTargetHasNoCorrespondingUkm) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetModelVersionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN, 2);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsOptimizationTargetModelPredictionScore) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 0.123);
  data.RecordMetrics(/*has_committed=*/false);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kPainfulPageLoadModelPredictionScoreName));
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm::builders::OptimizationGuide::
                                     kPainfulPageLoadModelPredictionScoreName,
                                 12);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsModelPredicitonScoreOptimizationTargetHasNoCorrespondingUkm) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN, 0.123);
  data.RecordMetrics(/*has_committed=*/false);

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsMultipleOptimizationTypes) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::DEFER_ALL_SCRIPT,
      optimization_guide::OptimizationTypeDecision::
          kAllowedByOptimizationFilter);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(
          optimization_guide::OptimizationTypeDecision::kAllowedByHint),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.DeferAllScript",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kAllowedByOptimizationFilter),
      1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsRecordsLatestType) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::
          kAllowedByOptimizationFilter);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kAllowedByOptimizationFilter),
      1);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsMultipleOptimizationTargets) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN,
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.Unknown",
      static_cast<int>(optimization_guide::OptimizationTargetDecision::
                           kPageLoadDoesNotMatch),
      1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsRecordsLatestTarget) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data.RecordMetrics(/*has_committed=*/false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
}

TEST(OptimizationGuideNavigationDataTest, DeepCopy) {
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  EXPECT_EQ(3, data->navigation_id());
  EXPECT_EQ(base::nullopt, data->serialized_hint_version_string());
  EXPECT_EQ(base::nullopt, data->GetDecisionForOptimizationType(
                               optimization_guide::proto::NOSCRIPT));
  EXPECT_EQ(
      base::nullopt,
      data->GetDecisionForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(base::nullopt, data->has_hint_before_commit());
  EXPECT_EQ(base::nullopt, data->has_hint_after_commit());
  EXPECT_FALSE(data->has_page_hint_value());
  EXPECT_EQ(
      base::nullopt,
      data->GetModelVersionForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(
      base::nullopt,
      data->GetModelPredictionScoreForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  data->set_serialized_hint_version_string("123abc");
  data->SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data->SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data->set_serialized_hint_version_string("123abc");
  data->set_has_hint_before_commit(true);
  data->set_has_hint_after_commit(true);
  optimization_guide::proto::PageHint page_hint;
  page_hint.set_page_pattern("pagepattern");
  data->set_page_hint(
      std::make_unique<optimization_guide::proto::PageHint>(page_hint));
  data->SetModelVersionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 123);
  data->SetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 0.12);

  OptimizationGuideNavigationData data_copy(*data);
  EXPECT_EQ(3, data_copy.navigation_id());
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            *data_copy.GetDecisionForOptimizationType(
                optimization_guide::proto::NOSCRIPT));
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
      *data_copy.GetDecisionForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(data_copy.has_hint_before_commit().value());
  EXPECT_TRUE(data_copy.has_hint_after_commit().value());
  EXPECT_EQ("123abc", *(data_copy.serialized_hint_version_string()));
  EXPECT_TRUE(data_copy.has_page_hint_value());
  EXPECT_EQ("pagepattern", data_copy.page_hint()->page_pattern());
  EXPECT_EQ(
      123,
      *data_copy.GetModelVersionForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(
      0.12,
      *data_copy.GetModelPredictionScoreForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
}
