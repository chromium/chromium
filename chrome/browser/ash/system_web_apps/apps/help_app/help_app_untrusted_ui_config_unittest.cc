// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_untrusted_ui_config.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/service/variations_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class HelpAppUntrustedUiConfigCrosSwitcherTest : public testing::Test {
 public:
  HelpAppUntrustedUiConfigCrosSwitcherTest() {
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, &enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());
  }

 private:
  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/false,
                                                            /*enabled=*/false};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<variations::VariationsService> variations_service_;
};

}  // namespace

TEST_F(HelpAppUntrustedUiConfigCrosSwitcherTest, FeatureListNotAvailable) {
  EXPECT_FALSE(HelpAppUntrustedUIConfig::IsCrosSwitcherEnabledForTesting(
      /*feature_list=*/nullptr, variations_service_.get()))
      << "Switcher is disabled for fail safe if feature_list instance is not "
         "provided.";
}

TEST_F(HelpAppUntrustedUiConfigCrosSwitcherTest, FeatureFlagOverriden) {
  base::FeatureList feature_list;
  variations_service_->OverrideStoredPermanentCountry("zz");
  feature_list.RegisterFieldTrialOverride(
      ash::features::kCrosSwitcher.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      base::FieldTrialList::CreateFieldTrial("CrosSwitcher", "GroupName"));
  EXPECT_TRUE(HelpAppUntrustedUIConfig::IsCrosSwitcherEnabledForTesting(
      &feature_list, variations_service_.get()))
      << "Expect that Switcher is enabled while specified country is not in "
         "the available countries list as the flag is overridden to enabled.";
}

TEST_F(HelpAppUntrustedUiConfigCrosSwitcherTest,
       VariationsServiceNotAvailable) {
  EXPECT_FALSE(HelpAppUntrustedUIConfig::IsCrosSwitcherEnabledForTesting(
      base::FeatureList::GetInstance(), /*variations_service=*/nullptr))
      << "Switcher is disabled for fail safe if variations_service instance is "
         "not provided.";
}

TEST_F(HelpAppUntrustedUiConfigCrosSwitcherTest, NotInAvailableCountry) {
  variations_service_->OverrideStoredPermanentCountry("zz");
  EXPECT_FALSE(HelpAppUntrustedUIConfig::IsCrosSwitcherEnabledForTesting(
      base::FeatureList::GetInstance(), variations_service_.get()))
      << "Switcher is not available if stored permanent country is not in "
         "available countries list.";
}

TEST_F(HelpAppUntrustedUiConfigCrosSwitcherTest, Available) {
  variations_service_->OverrideStoredPermanentCountry("us");
  EXPECT_TRUE(HelpAppUntrustedUIConfig::IsCrosSwitcherEnabledForTesting(
      base::FeatureList::GetInstance(), variations_service_.get()));
}

}  // namespace ash
