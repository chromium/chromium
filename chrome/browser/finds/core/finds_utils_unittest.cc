// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace finds {

using SuggestionTheme =
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme;

class FindsUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    FindsService::RegisterProfilePrefs(prefs_.registry());
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(FindsUtilsTest, ThemeTypeEnumToString) {
  EXPECT_EQ("EventsAndActivities",
            ThemeTypeEnumToString(SuggestionTheme::EVENTS_AND_ACTIVITIES));
  EXPECT_EQ("FoodAndDining",
            ThemeTypeEnumToString(SuggestionTheme::FOOD_AND_DINING));
  EXPECT_EQ("Entertainment",
            ThemeTypeEnumToString(SuggestionTheme::ENTERTAINMENT));
  EXPECT_EQ("Shopping", ThemeTypeEnumToString(SuggestionTheme::SHOPPING));
  EXPECT_EQ("Travel", ThemeTypeEnumToString(SuggestionTheme::TRAVEL));
  EXPECT_EQ("", ThemeTypeEnumToString(SuggestionTheme::UNKNOWN));
}

TEST_F(FindsUtilsTest, MarkThemeAsNotInterested) {
  const base::DictValue& not_interested_themes =
      prefs()->GetDict(prefs::kFindsNotInterestedThemesLastTimestamp);
  EXPECT_TRUE(not_interested_themes.empty());

  MarkThemeAsNotInterested(prefs(), SuggestionTheme::SHOPPING);

  const base::DictValue& updated_themes =
      prefs()->GetDict(prefs::kFindsNotInterestedThemesLastTimestamp);
  EXPECT_TRUE(updated_themes.contains("Shopping"));
}

TEST_F(FindsUtilsTest, MarkModelExecutionLastTimestamp) {
  EXPECT_EQ(0, prefs()->GetInt64(prefs::kFindsModelExecutionLastTimestamp));

  MarkModelExecutionLastTimestamp(prefs());

  EXPECT_GT(prefs()->GetInt64(prefs::kFindsModelExecutionLastTimestamp), 0);
}

TEST_F(FindsUtilsTest, IsAllowedByEnterprisePolicy) {
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(
      prefs()->registry());

  EXPECT_TRUE(IsAllowedByEnterprisePolicy(prefs()));

  prefs()->SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kAllow));
  EXPECT_TRUE(IsAllowedByEnterprisePolicy(prefs()));

  // Should return false when the enterprise policy is explicitly disabled.
  prefs()->SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));
  EXPECT_FALSE(IsAllowedByEnterprisePolicy(prefs()));
}
TEST_F(FindsUtilsTest, IsHistorySyncAndMsbbEnabled) {
  prefs()->registry()->RegisterBooleanPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  syncer::TestSyncService sync_service;

  // Both History Sync and MSBB disabled.
  sync_service.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  prefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  EXPECT_FALSE(IsHistorySyncAndMsbbEnabled(&sync_service, prefs()));

  // Only History Sync enabled.
  sync_service.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_FALSE(IsHistorySyncAndMsbbEnabled(&sync_service, prefs()));

  // Only MSBB enabled.
  sync_service.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  prefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  EXPECT_FALSE(IsHistorySyncAndMsbbEnabled(&sync_service, prefs()));

  // Both History Sync and MSBB enabled.
  sync_service.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_TRUE(IsHistorySyncAndMsbbEnabled(&sync_service, prefs()));
}

}  // namespace finds
