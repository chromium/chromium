// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Tests setting the visibility of the History Clusters by policy.
class HistoryClustersPolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();

    config_.is_journeys_enabled_no_locale_check = true;
    history_clusters::SetConfigForTesting(config_);
  }

 private:
  history_clusters::Config config_;
};

IN_PROC_BROWSER_TEST_F(HistoryClustersPolicyTest, HistoryClustersVisible) {
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(browser()->profile());
  PrefService* prefs = browser()->profile()->GetPrefs();
  PolicyMap policies;

  // Verify that history clusters are visible by default.
  EXPECT_TRUE(prefs->GetBoolean(history_clusters::prefs::kVisible));
  EXPECT_FALSE(prefs->IsManagedPreference(history_clusters::prefs::kVisible));
  EXPECT_TRUE(history_clusters_service->IsJourneysEnabledAndVisible());

  // Verify that history clusters can be hidden by prefs.
  prefs->SetBoolean(history_clusters::prefs::kVisible, false);

  EXPECT_FALSE(prefs->GetBoolean(history_clusters::prefs::kVisible));
  EXPECT_FALSE(prefs->IsManagedPreference(history_clusters::prefs::kVisible));
  // History clusters are always visible unless the visibility prefs is set to
  // false by policy.
  EXPECT_TRUE(history_clusters_service->IsJourneysEnabledAndVisible());

  // Verify that history clusters can be hidden by policy.
  policies.Set(key::kHistoryClustersVisible, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(history_clusters::prefs::kVisible));
  EXPECT_TRUE(prefs->IsManagedPreference(history_clusters::prefs::kVisible));
  EXPECT_FALSE(history_clusters_service->IsJourneysEnabledAndVisible());

  // Verify that history clusters can be made visible by policy.
  policies.Set(key::kHistoryClustersVisible, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(prefs->GetBoolean(history_clusters::prefs::kVisible));
  EXPECT_TRUE(prefs->IsManagedPreference(history_clusters::prefs::kVisible));
  EXPECT_TRUE(history_clusters_service->IsJourneysEnabledAndVisible());

  // Verify that clearing the policy restores the original prefs.
  policies.Clear();
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(history_clusters::prefs::kVisible));
  EXPECT_FALSE(prefs->IsManagedPreference(history_clusters::prefs::kVisible));
  // History clusters are always visible unless the visibility prefs is set to
  // false by policy.
  EXPECT_TRUE(history_clusters_service->IsJourneysEnabledAndVisible());
}

}  // namespace policy
