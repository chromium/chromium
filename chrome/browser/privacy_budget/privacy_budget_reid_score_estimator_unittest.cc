// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/privacy_budget/privacy_budget_reid_score_estimator.h"
#include <cstdint>

#include "base/barrier_closure.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/scoped_identifiability_test_sample_collector.h"

TEST(PrivacyBudgetReidScoreEstimatorStandaloneTest,
     ReportReidFixedTokenRandomSalt) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "", "", "", "2077075229;1122849309", "1000000", "1");

  constexpr auto surface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  constexpr auto surface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);

  int64_t token1 = 1234;
  int64_t token2 = 12345;
  constexpr int num_iterations = 50;
  base::test::SingleThreadTaskEnvironment task_environment;
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      base::BarrierClosure(num_iterations, run_loop.QuitClosure()));
  blink::test::ScopedIdentifiabilityTestSampleCollector collector;
  for (int i = 0; i < num_iterations; ++i) {
    auto reid_storage =
        std::make_unique<PrivacyBudgetReidScoreEstimator>(settings);
    // Process values for 2 surfaces.
    reid_storage->ProcessForReidScore(surface_1,
                                      blink::IdentifiableToken(token1));
    reid_storage->ProcessForReidScore(surface_2,
                                      blink::IdentifiableToken(token2));
  }
  // This should let the async tasks run.
  run_loop.Run();
  const auto& entries = collector.entries();
  bool has_value_0 = false;
  bool has_value_1 = false;
  int count = 0;
  for (auto& entry : entries) {
    for (auto& metric : entry.metrics) {
      auto surface = metric.surface;
      if (surface.GetType() ==
          blink::IdentifiableSurface::Type::kReidScoreEstimator) {
        EXPECT_EQ(metric.surface.ToUkmMetricHash(), 7415899889871487013u);
        ++count;
        uint64_t hash = static_cast<uint64_t>(metric.value.ToUkmMetricValue());
        uint32_t reid_bits = hash & 0xFFFFFFFF;
        EXPECT_TRUE(reid_bits == 0 || reid_bits == 1);
        if (reid_bits == 0)
          has_value_0 = true;
        else if (reid_bits == 1)
          has_value_1 = true;
        uint32_t salt = (hash >> 32);
        EXPECT_LT(salt, 1000000u);
      }
    }
  }
  EXPECT_EQ(count, num_iterations);
  // Since the 1 bit should be random, the probability of it being always 0 or
  // always 1 is 2/(2^num_iterations), hence it should be negligible.
  EXPECT_TRUE(has_value_0);
  EXPECT_TRUE(has_value_1);
}

TEST(PrivacyBudgetReidScoreEstimatorStandaloneTest,
     ReportReidRandomTokenFixedSalt) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "", "", "", "2077075229;1122849309", "1", "1");
  constexpr auto surface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  constexpr auto surface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);
  constexpr int num_iterations = 50;
  base::test::SingleThreadTaskEnvironment task_environment;
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      base::BarrierClosure(num_iterations, run_loop.QuitClosure()));
  blink::test::ScopedIdentifiabilityTestSampleCollector collector;
  for (int i = 0; i < num_iterations; ++i) {
    auto reid_storage =
        std::make_unique<PrivacyBudgetReidScoreEstimator>(settings);
    // Create random tokens.
    int64_t token1 = static_cast<int64_t>(base::RandUint64());
    int64_t token2 = static_cast<int64_t>(base::RandUint64());
    // Process values for 2 surfaces.
    reid_storage->ProcessForReidScore(surface_1,
                                      blink::IdentifiableToken(token1));
    reid_storage->ProcessForReidScore(surface_2,
                                      blink::IdentifiableToken(token2));
  }
  // This should let the async tasks run.
  run_loop.Run();
  const auto& entries = collector.entries();
  bool has_value_0 = false;
  bool has_value_1 = false;
  int count = 0;
  for (auto& entry : entries) {
    for (auto& metric : entry.metrics) {
      auto surface = metric.surface;
      if (surface.GetType() ==
          blink::IdentifiableSurface::Type::kReidScoreEstimator) {
        EXPECT_EQ(metric.surface.ToUkmMetricHash(), 7415899889871487013u);
        ++count;
        uint64_t hash = static_cast<uint64_t>(metric.value.ToUkmMetricValue());
        uint32_t reid_bits = hash & 0xFFFFFFFF;
        EXPECT_TRUE(reid_bits == 0 || reid_bits == 1);
        if (reid_bits == 0)
          has_value_0 = true;
        else if (reid_bits == 1)
          has_value_1 = true;
        uint32_t salt = (hash >> 32);
        EXPECT_EQ(salt, 0u);
      }
    }
  }
  EXPECT_EQ(count, num_iterations);
  // Since the 1 bit should be random, the probability of it being always 0 or
  // always 1 is 2/(2^num_iterations), hence it should be negligible.
  EXPECT_TRUE(has_value_0);
  EXPECT_TRUE(has_value_1);
}
