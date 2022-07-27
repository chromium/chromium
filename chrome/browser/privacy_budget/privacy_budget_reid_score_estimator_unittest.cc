// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_reid_score_estimator.h"

#include <cstdint>

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/scoped_identifiability_test_sample_collector.h"

class PrivacyBudgetReidScoreEstimatorStandaloneTest : public ::testing::Test {
 public:
  PrivacyBudgetReidScoreEstimatorStandaloneTest() {
    prefs::RegisterPrivacyBudgetPrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  TestingPrefServiceSimple pref_service_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PrivacyBudgetReidScoreEstimatorStandaloneTest,
       ReidEstimatorWrongParameters) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      /*enabled=*/true,
      /*expected_surface_count=*/0,
      /*surface_budget=*/0,
      /*blocks=*/"",
      /*blocks_weights=*/"",
      /*allowed_random_types=*/"",
      /*reid_blocks=*/"2077075229;1122849309,2077075230;1122849310",
      /*reid_blocks_salts_ranges=*/"1000000", /*Missing Salt!*/
      /*reid_blocks_bits=*/"1,2",
      /*reid_blocks_noise_probabilities=*/"0,0");

  PrivacyBudgetReidScoreEstimator reid_storage(&settings, pref_service());
  reid_storage.ResetPersistedState();
  // Test passes if initializing the Reid estimator is skipped and does not
  // crash.
  reid_storage.Init();
}

TEST_F(PrivacyBudgetReidScoreEstimatorStandaloneTest,
       ReportReidFixedTokenRandomSalt) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      /*enabled=*/true,
      /*expected_surface_count=*/0,
      /*surface_budget=*/0,
      /*blocks=*/"",
      /*blocks_weights=*/"",
      /*allowed_random_types=*/"",
      /*reid_blocks=*/"2077075229;1122849309",
      /*reid_blocks_salts_ranges=*/"1000000",
      /*reid_blocks_bits=*/"1",
      /*reid_blocks_noise_probabilities=*/"0");

  constexpr auto surface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  constexpr auto surface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);

  int64_t token1 = 1234;
  int64_t token2 = 12345;
  constexpr int num_iterations = 50;
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      base::BarrierClosure(num_iterations, run_loop.QuitClosure()));
  blink::test::ScopedIdentifiabilityTestSampleCollector collector;
  for (int i = 0; i < num_iterations; ++i) {
    PrivacyBudgetReidScoreEstimator reid_storage(&settings, pref_service());
    reid_storage.ResetPersistedState();
    reid_storage.Init();
    // Process values for 2 surfaces.
    reid_storage.ProcessForReidScore(surface_1,
                                     blink::IdentifiableToken(token1));
    reid_storage.ProcessForReidScore(surface_2,
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
        EXPECT_EQ(metric.surface.ToUkmMetricHash(), 11332616172707669541u);
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

TEST_F(PrivacyBudgetReidScoreEstimatorStandaloneTest,
       ReportReidRandomTokenFixedSalt) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      /*enabled=*/true,
      /*expected_surface_count=*/0,
      /*surface_budget=*/0,
      /*blocks=*/"",
      /*blocks_weights=*/"",
      /*allowed_random_types=*/"",
      /*reid_blocks=*/"2077075229;1122849309",
      /*reid_blocks_salts_ranges=*/"1",
      /*reid_blocks_bits=*/"1",
      /*reid_blocks_noise_probabilities=*/"0");

  constexpr auto surface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  constexpr auto surface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);
  constexpr int num_iterations = 50;
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      base::BarrierClosure(num_iterations, run_loop.QuitClosure()));
  blink::test::ScopedIdentifiabilityTestSampleCollector collector;
  for (int i = 0; i < num_iterations; ++i) {
    PrivacyBudgetReidScoreEstimator reid_storage(&settings, pref_service());
    reid_storage.ResetPersistedState();
    reid_storage.Init();
    // Create random tokens.
    int64_t token1 = static_cast<int64_t>(base::RandUint64());
    int64_t token2 = static_cast<int64_t>(base::RandUint64());
    // Process values for 2 surfaces.
    reid_storage.ProcessForReidScore(surface_1,
                                     blink::IdentifiableToken(token1));
    reid_storage.ProcessForReidScore(surface_2,
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
        EXPECT_EQ(metric.surface.ToUkmMetricHash(), 11332616172707669541u);
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

TEST_F(PrivacyBudgetReidScoreEstimatorStandaloneTest,
       ReportReidFixedTokenFixedSaltAllNoise) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      /*enabled=*/true,
      /*expected_surface_count=*/0,
      /*surface_budget=*/0,
      /*blocks=*/"",
      /*blocks_weights=*/"",
      /*allowed_random_types=*/"",
      /*reid_blocks=*/"2077075229;1122849309",
      /*reid_blocks_salts_ranges=*/"1",
      /*reid_blocks_bits=*/"32",
      /*reid_blocks_noise_probabilities=*/"1");

  constexpr auto surface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  constexpr auto surface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);

  int64_t token1 = 1234;
  int64_t token2 = 12345;
  constexpr int num_iterations = 50;
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      base::BarrierClosure(num_iterations, run_loop.QuitClosure()));
  blink::test::ScopedIdentifiabilityTestSampleCollector collector;
  for (int i = 0; i < num_iterations; ++i) {
    PrivacyBudgetReidScoreEstimator reid_storage(&settings, pref_service());
    reid_storage.ResetPersistedState();
    reid_storage.Init();
    // Process values for 2 surfaces.
    reid_storage.ProcessForReidScore(surface_1,
                                     blink::IdentifiableToken(token1));
    reid_storage.ProcessForReidScore(surface_2,
                                     blink::IdentifiableToken(token2));
  }
  // This should let the async tasks run.
  run_loop.Run();
  const auto& entries = collector.entries();
  base::flat_set<uint32_t> reid_results;
  int count = 0;
  for (auto& entry : entries) {
    for (auto& metric : entry.metrics) {
      auto surface = metric.surface;
      if (surface.GetType() ==
          blink::IdentifiableSurface::Type::kReidScoreEstimator) {
        EXPECT_EQ(metric.surface.ToUkmMetricHash(), 11332616172707669541u);
        ++count;
        uint64_t hash = static_cast<uint64_t>(metric.value.ToUkmMetricValue());
        uint32_t reid_bits = hash & 0xFFFFFFFF;
        // Result should be noise i.e. didn't appeared before.
        EXPECT_FALSE(reid_results.contains(reid_bits));
        reid_results.insert(reid_bits);
        uint32_t salt = (hash >> 32);
        EXPECT_EQ(salt, 0u);
      }
    }
  }
  EXPECT_EQ(count, num_iterations);
}

TEST_F(PrivacyBudgetReidScoreEstimatorStandaloneTest,
       ReidHashIsReportedOnlyOnce) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      /*enabled=*/true,
      /*expected_surface_count=*/0,
      /*surface_budget=*/0,
      /*blocks=*/"",
      /*blocks_weights=*/"",
      /*allowed_random_types=*/"",
      /*reid_blocks=*/"2077075229;1122849309",
      /*reid_blocks_salts_ranges=*/"1000000",
      /*reid_blocks_bits=*/"1",
      /*reid_blocks_noise_probabilities=*/"0");

  constexpr auto surface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  constexpr auto surface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);

  ukm::TestAutoSetUkmRecorder test_recorder;

  {
    PrivacyBudgetReidScoreEstimator reid_storage(&settings, pref_service());
    reid_storage.Init();

    {
      base::RunLoop run_loop;
      test_recorder.SetOnAddEntryCallback(
          ukm::builders::Identifiability::kEntryName, run_loop.QuitClosure());
      blink::test::ScopedIdentifiabilityTestSampleCollector collector;

      // Process values for 2 surfaces.
      reid_storage.ProcessForReidScore(surface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(surface_2, blink::IdentifiableToken(2));

      // This should let the async tasks run.
      run_loop.Run();

      const auto& entries = collector.entries();
      EXPECT_EQ(entries.size(), 1u);
      EXPECT_EQ(entries[0].metrics.size(), 1u);
      EXPECT_EQ(entries[0].metrics[0].surface.ToUkmMetricHash(),
                11332616172707669541u);
    }

    // Now check that the reid hash is not reported again if we see again the
    // two surfaces.
    {
      blink::test::ScopedIdentifiabilityTestSampleCollector collector;

      reid_storage.ProcessForReidScore(surface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(surface_2, blink::IdentifiableToken(2));

      RunUntilIdle();
      const auto& entries = collector.entries();
      EXPECT_TRUE(entries.empty());
    }
  }

  // Even if we instantiate a new PrivacyBudgetReidScoreEstimator, the Reid
  // hash is not reported again because of the information persisted in the
  // PrefService.
  {
    PrivacyBudgetReidScoreEstimator reid_storage(&settings, pref_service());
    reid_storage.Init();
    {
      blink::test::ScopedIdentifiabilityTestSampleCollector collector;

      reid_storage.ProcessForReidScore(surface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(surface_2, blink::IdentifiableToken(2));

      RunUntilIdle();

      const auto& entries = collector.entries();
      EXPECT_TRUE(entries.empty());
    }

    // If we reset the persisted state, then the Reid hash will be reported
    // again.
    reid_storage.ResetPersistedState();
    reid_storage.Init();

    {
      base::RunLoop run_loop;
      test_recorder.SetOnAddEntryCallback(
          ukm::builders::Identifiability::kEntryName, run_loop.QuitClosure());

      blink::test::ScopedIdentifiabilityTestSampleCollector collector;
      reid_storage.ProcessForReidScore(surface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(surface_2, blink::IdentifiableToken(2));

      run_loop.Run();

      const auto& entries = collector.entries();
      EXPECT_EQ(entries.size(), 1u);
      EXPECT_EQ(entries[0].metrics.size(), 1u);
      EXPECT_EQ(entries[0].metrics[0].surface.ToUkmMetricHash(),
                11332616172707669541u);
    }
  }
}
