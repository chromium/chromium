// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_metrics_provider.h"

#include <memory>

#include "chrome/browser/privacy_budget/inspectable_identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrivacyBudgetMetricsProvider, Init) {
  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();

  test::ScopedPrivacyBudgetConfig::Parameters params;

  // The study is enabled, but the probability of any surface being selected is
  // quite low.
  params.enabled = true;
  params.surface_selection_rate =
      features::kMaxIdentifiabilityStudySurfaceSelectionRate;
  test::ScopedPrivacyBudgetConfig scoped_config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
  const blink::IdentifiableSurface kSomeSurface =
      blink::IdentifiableSurface::FromMetricHash(100);

  // kSomeSurface should not be selected.
  ASSERT_FALSE(state.ShouldRecordSurface(kSomeSurface));

  // But we'll poke it into the active surface set.
  pref_service.SetString(prefs::kPrivacyBudgetActiveSurfaces, "100");

  // Init() should re-initialize the study state using the newly set pref value.
  auto metrics_provider =
      std::make_unique<PrivacyBudgetMetricsProvider>(&state);
  metrics_provider->Init();

  EXPECT_TRUE(state.ShouldRecordSurface(kSomeSurface));
}

TEST(PrivacyBudgetMetricsProvider, OnClientStateClearedWithStudyEnabled) {
  const int kWrongGeneration = 2;
  const int kCorrectGeneration = 1;

  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  test::ScopedPrivacyBudgetConfig::Parameters params;
  params.enabled = true;
  params.generation = kCorrectGeneration;
  test::ScopedPrivacyBudgetConfig scoped_config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration, kWrongGeneration);

  auto metrics_provider =
      std::make_unique<PrivacyBudgetMetricsProvider>(&state);
  metrics_provider->OnClientStateCleared();

  EXPECT_EQ(kCorrectGeneration,
            pref_service.GetInteger(prefs::kPrivacyBudgetGeneration));
}

TEST(PrivacyBudgetMetricsProvider, OnClientStateClearedWithStudyDisabled) {
  const int kWrongGeneration = 2;

  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  test::ScopedPrivacyBudgetConfig::Parameters params;
  params.enabled = false;
  params.generation = kWrongGeneration;
  test::ScopedPrivacyBudgetConfig scoped_config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration, kWrongGeneration);

  auto metrics_provider =
      std::make_unique<PrivacyBudgetMetricsProvider>(&state);
  metrics_provider->OnClientStateCleared();

  // 0 is the default value specified when the kPrivacyBudgetGeneration pref is
  // registered.
  EXPECT_EQ(0, pref_service.GetInteger(prefs::kPrivacyBudgetGeneration));
}
