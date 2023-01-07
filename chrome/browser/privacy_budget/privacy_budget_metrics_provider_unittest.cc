// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_metrics_provider.h"

#include <memory>

#include "base/containers/contains.h"
#include "chrome/browser/privacy_budget/inspectable_identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

TEST(PrivacyBudgetMetricsProvider, InitOnConstruction) {
  test::ScopedPrivacyBudgetConfig scoped_config(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration,
                          test::ScopedPrivacyBudgetConfig::kDefaultGeneration);
  pref_service.SetString(prefs::kPrivacyBudgetSeenSurfaces, "100");

  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(base::Contains(state.seen_surfaces(),
                             blink::IdentifiableSurface::FromMetricHash(100)));
}

TEST(PrivacyBudgetMetricsProvider, OnClientStateClearedWithStudyEnabled) {
  const int kWrongGeneration = 2;
  const int kCorrectGeneration = 1;

  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  test::ScopedPrivacyBudgetConfig::Parameters params(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
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
  test::ScopedPrivacyBudgetConfig::Parameters params(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);
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
