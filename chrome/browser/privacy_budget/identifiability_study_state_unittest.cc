// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_state.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/privacy_budget/inspectable_identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

// Constants used to set up the test configuration.
constexpr int kTestingGeneration = 58;
constexpr auto kBlockedSurface1 = blink::IdentifiableSurface::FromMetricHash(1);
constexpr auto kFakeSeed = UINT64_C(9);
constexpr auto kReallyBigSeed = std::numeric_limits<uint64_t>::max() - 100u;
constexpr auto kBlockedType1 =
    blink::IdentifiableSurface::Type::kCanvasReadback;

// Sample surfaces. "Regular" means that the surface is not blocked and its type
// is not blocked.
constexpr auto kRegularSurface1 =
    blink::IdentifiableSurface::FromMetricHash(3 << 8);
constexpr auto kRegularSurface2 =
    blink::IdentifiableSurface::FromMetricHash(4 << 8);
constexpr auto kRegularSurface3 =
    blink::IdentifiableSurface::FromMetricHash(5 << 8);

constexpr auto kBlockedTypeSurface1 =
    blink::IdentifiableSurface::FromTypeAndToken(kBlockedType1, 1);  // = 258

std::string SurfaceListString(
    std::initializer_list<blink::IdentifiableSurface> list) {
  std::vector<std::string> list_as_strings(list.size());
  std::transform(
      list.begin(), list.end(), list_as_strings.begin(),
      [](const auto& v) { return base::NumberToString(v.ToUkmMetricHash()); });
  return base::JoinString(list_as_strings, ",");
}

// Make names short
using IdentifiableSurfaceSet =
    test_utils::InspectableIdentifiabilityStudyState::IdentifiableSurfaceSet;
using IdentifiableSurfaceTypeSet = test_utils::
    InspectableIdentifiabilityStudyState::IdentifiableSurfaceTypeSet;

}  // namespace

class IdentifiabilityStudyStateTest : public ::testing::Test {
 public:
  IdentifiabilityStudyStateTest() {
    // Uses FeatureLists. Hence needs to be initialized in the constructor lest
    // we add any multithreading tests here.
    auto parameters = test::ScopedPrivacyBudgetConfig::Parameters{};
    parameters.generation = kTestingGeneration;
    parameters.blocked_surfaces.push_back(kBlockedSurface1);
    parameters.blocked_types.push_back(kBlockedType1);
    config_.Apply(parameters);
    prefs::RegisterPrivacyBudgetPrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  test::ScopedPrivacyBudgetConfig config_;
};

TEST_F(IdentifiabilityStudyStateTest, InstantiateAndInitialize) {
  auto settings = std::make_unique<IdentifiabilityStudyState>(pref_service());

  // Successful initialization should result in a new PRNG seed and setting the
  // generation number.
  EXPECT_EQ(kTestingGeneration,
            pref_service()->GetInteger(prefs::kPrivacyBudgetGeneration));
  EXPECT_NE(UINT64_C(0), pref_service()->GetUint64(prefs::kPrivacyBudgetSeed));
}

TEST_F(IdentifiabilityStudyStateTest, ReInitializeWhenGenerationChanges) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration - 1);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);

  auto settings = std::make_unique<IdentifiabilityStudyState>(pref_service());

  // Successful re-initialization should result in a new PRNG seed and setting
  // the generation number.
  EXPECT_EQ(kTestingGeneration,
            pref_service()->GetInteger(prefs::kPrivacyBudgetGeneration));
  EXPECT_NE(kFakeSeed, pref_service()->GetUint64(prefs::kPrivacyBudgetSeed));
}

TEST_F(IdentifiabilityStudyStateTest, ReallyBigSeed) {
  // The thing being tested is whether we can store and restore a 64-bit int
  // that's larger than the largest signed integer. This ensures that the
  // correct unsigned 64-bit type is used in code to store and restore the seed.
  static_assert(
      kReallyBigSeed > std::numeric_limits<int64_t>::max(),
      "kReallyBigSeed must be larger than the largest signed 64-bit int");
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kReallyBigSeed);

  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());

  EXPECT_EQ(kReallyBigSeed, settings->prng_seed());
}

TEST_F(IdentifiabilityStudyStateTest, BadSeed) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetString(prefs::kPrivacyBudgetSeed, "-1");

  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());

  EXPECT_NE(0u, settings->prng_seed());
}

TEST_F(IdentifiabilityStudyStateTest, LoadsFromPrefs) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString({kRegularSurface1, kRegularSurface2}));
  pref_service()->SetString(prefs::kPrivacyBudgetRetiredSurfaces,
                            SurfaceListString({kBlockedTypeSurface1}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2}),
            settings->active_surfaces());
  EXPECT_EQ((IdentifiableSurfaceSet{kBlockedTypeSurface1}),
            settings->retired_surfaces());
  EXPECT_EQ(kFakeSeed, settings->prng_seed());
  EXPECT_EQ(kTestingGeneration, settings->generation());
}

TEST_F(IdentifiabilityStudyStateTest, ReconcileBlockedSurfaces) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString(
          {kBlockedSurface1, kRegularSurface1, kRegularSurface2}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2}),
            settings->active_surfaces());
  EXPECT_EQ((IdentifiableSurfaceSet{kBlockedSurface1}),
            settings->retired_surfaces());
}

TEST_F(IdentifiabilityStudyStateTest, ReconcileBlockedTypes) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString(
          {kBlockedTypeSurface1, kRegularSurface1, kRegularSurface2}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2}),
            settings->active_surfaces());
  EXPECT_EQ((IdentifiableSurfaceSet{kBlockedTypeSurface1}),
            settings->retired_surfaces());
  EXPECT_EQ("289",
            pref_service()->GetString(prefs::kPrivacyBudgetRetiredSurfaces));
}

TEST_F(IdentifiabilityStudyStateTest, AllowsActive) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString(
          {kRegularSurface1, kRegularSurface2, kRegularSurface3}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface1));
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface2));
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface3));
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2,
                                    kRegularSurface3}),
            settings->active_surfaces());
}

TEST_F(IdentifiabilityStudyStateTest, BlocksBlocked) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString({kRegularSurface1, kRegularSurface2}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_FALSE(settings->ShouldRecordSurface(kBlockedSurface1));
  EXPECT_FALSE(settings->ShouldRecordSurface(kBlockedTypeSurface1));
}

TEST_F(IdentifiabilityStudyStateTest, UpdatesActive) {
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface1));
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1}),
            settings->active_surfaces());
  EXPECT_EQ(SurfaceListString({kRegularSurface1}),
            pref_service()->GetString(prefs::kPrivacyBudgetActiveSurfaces));
}

// Verify that the study parameters don't overflow.
TEST(IdentifiabilityStudyStateStandaloneTest, HighClamps) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.max_surfaces = features::kMaxIdentifiabilityStudyMaxSurfaces + 1;
  params.surface_selection_rate =
      features::kMaxIdentifiabilityStudySurfaceSelectionRate + 1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_EQ(features::kMaxIdentifiabilityStudyMaxSurfaces,
            settings.max_active_surfaces());
  EXPECT_EQ(features::kMaxIdentifiabilityStudySurfaceSelectionRate,
            settings.surface_selection_rate());
}

// Verify that the study parameters don't underflow.
TEST(IdentifiabilityStudyStateStandaloneTest, LowClamps) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.max_surfaces = -1;
  params.surface_selection_rate = -1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_EQ(0, settings.max_active_surfaces());
  EXPECT_EQ(0, settings.surface_selection_rate());
}

TEST(IdentifiabilityStudyStateStandaloneTest, Disabled) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.enabled = false;
  params.surface_selection_rate = 1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface1));
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface2));
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface3));
}

TEST(IdentifiabilityStudyStateStandaloneTest, DisabledStudyDoesNotNukePrefs) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.enabled = false;
  params.surface_selection_rate = 1;
  test::ScopedPrivacyBudgetConfig config(params);

  const std::string kSurfaces = "1,2,3";

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  pref_service.SetString(prefs::kPrivacyBudgetActiveSurfaces, kSurfaces);
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_EQ(kSurfaces,
            pref_service.GetString(prefs::kPrivacyBudgetActiveSurfaces));
}
