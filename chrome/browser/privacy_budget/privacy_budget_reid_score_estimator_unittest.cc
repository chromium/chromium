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

class PrivacyBudgetReidScoreEstimatorStandaloneTest
    : public ::testing::TestWithParam<std::tuple<uint32_t, int, int, bool>> {
 public:
  PrivacyBudgetReidScoreEstimatorStandaloneTest() {
    prefs::RegisterPrivacyBudgetPrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ProcessReidRecords(IdentifiabilityStudyGroupSettings* settings,
                          bool fixedToken) {
    for (int i = 0; i < kNumIterations; ++i) {
      PrivacyBudgetReidScoreEstimator reid_storage(settings, pref_service());
      reid_storage.ResetPersistedState();
      reid_storage.Init();
      int64_t token1 =
          fixedToken ? 1 : static_cast<int64_t>(base::RandUint64());
      int64_t token2 =
          fixedToken ? 2 : static_cast<int64_t>(base::RandUint64());
      // Process values for 2 surfaces.
      reid_storage.ProcessForReidScore(kSurface_1,
                                       blink::IdentifiableToken(token1));
      reid_storage.ProcessForReidScore(kSurface_2,
                                       blink::IdentifiableToken(token2));
    }
  }

  // Example surfaces for testing.
  const blink::IdentifiableSurface kSurface_1 =
      blink::IdentifiableSurface::FromMetricHash(2077075229u);
  const blink::IdentifiableSurface kSurface_2 =
      blink::IdentifiableSurface::FromMetricHash(1122849309u);

  // Expected Reid surface key to be reported based on the example surfaces
  // defined (kSurface_1, kSurface_2) using the function
  // IdentifiableSurface::FromTypeAndToken() with type
  // IdentifiableSurface::Type::kReidScoreEstimator.
  const uint64_t kExpectedSurface = 11332616172707669541u;

  const int kNumIterations = 50;

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

TEST_P(PrivacyBudgetReidScoreEstimatorStandaloneTest,
       ReportReidwithParameters) {
  const auto [salt_ranges, bits, noise, fixed_token] = GetParam();
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      /*enabled=*/true,
      /*expected_surface_count=*/0,
      /*surface_budget=*/0,
      /*blocks=*/"",
      /*blocks_weights=*/"",
      /*allowed_random_types=*/"",
      /*reid_blocks=*/"2077075229;1122849309",
      /*reid_blocks_salts_ranges=*/base::NumberToString(salt_ranges),
      /*reid_blocks_bits=*/base::NumberToString(bits),
      /*reid_blocks_noise_probabilities=*/base::NumberToString(noise));

  ukm::TestAutoSetUkmRecorder test_recorder;
  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      base::BarrierClosure(kNumIterations, run_loop.QuitClosure()));
  blink::test::ScopedIdentifiabilityTestSampleCollector collector;
  ProcessReidRecords(&settings, fixed_token);
  // This should let the async tasks run.
  run_loop.Run();
  const auto& entries = collector.entries();
  bool has_value_0 = false;
  bool has_value_1 = false;
  base::flat_set<uint32_t> reid_results;
  int count = 0;
  for (auto& entry : entries) {
    for (auto& metric : entry.metrics) {
      auto surface = metric.surface;
      if (surface.GetType() ==
          blink::IdentifiableSurface::Type::kReidScoreEstimator) {
        EXPECT_EQ(metric.surface.ToUkmMetricHash(), kExpectedSurface);
        ++count;
        uint64_t hash = static_cast<uint64_t>(metric.value.ToUkmMetricValue());
        uint32_t reid_bits = hash & 0xFFFFFFFF;
        if (noise == 1) {
          // Result should be noise i.e. didn't appear before.
          EXPECT_FALSE(reid_results.contains(reid_bits));
          reid_results.insert(reid_bits);
        } else {
          EXPECT_TRUE(reid_bits == 0 || reid_bits == 1);
          if (reid_bits == 0)
            has_value_0 = true;
          else if (reid_bits == 1)
            has_value_1 = true;
        }
        uint32_t salt = (hash >> 32);
        EXPECT_TRUE((salt >= 0) && (salt < salt_ranges));
      }
    }
  }
  EXPECT_EQ(count, kNumIterations);
  // Since the 1 bit should be random, the probability of it being always 0 or
  // always 1 is 2/(2^kNumIterations), hence it should be negligible.
  if (noise != 1) {
    EXPECT_TRUE(has_value_0);
    EXPECT_TRUE(has_value_1);
  }
}

INSTANTIATE_TEST_SUITE_P(PrivacyBudgetReidScoreEstimatorParameterizedTest,
                         PrivacyBudgetReidScoreEstimatorStandaloneTest,
                         ::testing::Values(std::make_tuple(1000000, 1, 0, true),
                                           std::make_tuple(1, 1, 0, false),
                                           std::make_tuple(1, 32, 1, true)));

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
      reid_storage.ProcessForReidScore(kSurface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(kSurface_2, blink::IdentifiableToken(2));

      // This should let the async tasks run.
      run_loop.Run();

      const auto& entries = collector.entries();
      EXPECT_EQ(entries.size(), 1u);
      EXPECT_EQ(entries[0].metrics.size(), 1u);
      EXPECT_EQ(entries[0].metrics[0].surface.ToUkmMetricHash(),
                kExpectedSurface);
    }

    // Now check that the reid hash is not reported again if we see again the
    // two surfaces.
    {
      blink::test::ScopedIdentifiabilityTestSampleCollector collector;

      reid_storage.ProcessForReidScore(kSurface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(kSurface_2, blink::IdentifiableToken(2));

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

      reid_storage.ProcessForReidScore(kSurface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(kSurface_2, blink::IdentifiableToken(2));

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
      reid_storage.ProcessForReidScore(kSurface_1, blink::IdentifiableToken(1));
      reid_storage.ProcessForReidScore(kSurface_2, blink::IdentifiableToken(2));

      run_loop.Run();

      const auto& entries = collector.entries();
      EXPECT_EQ(entries.size(), 1u);
      EXPECT_EQ(entries[0].metrics.size(), 1u);
      EXPECT_EQ(entries[0].metrics[0].surface.ToUkmMetricHash(),
                kExpectedSurface);
    }
  }
}
