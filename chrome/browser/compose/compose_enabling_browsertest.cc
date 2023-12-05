// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/compose/chrome_compose_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ComposeEnablingBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {
            optimization_guide::features::kOptimizationGuideModelExecution,
            optimization_guide::features::internal::kComposeSettingsVisibility,
        },
        {});
    InProcessBrowserTest::SetUp();
  }

  ComposeEnabling& GetComposeEnabling() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ChromeComposeClient* compose_client =
        ChromeComposeClient::FromWebContents(web_contents);
    return compose_client->GetComposeEnabling();
  }

  OptimizationGuideKeyedService* GetOptimizationGuide() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// PRE_ step simulates a browser restart.
IN_PROC_BROWSER_TEST_F(ComposeEnablingBrowserTest,
                       PRE_EnableComposeViaSettings) {
  // Sign-in.
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "test@example.com", signin::ConsentLevel::kSync);
  // Turn on MSBB.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Checks that Compose is disabled.
  EXPECT_NE(base::ok(),
            GetComposeEnabling().IsEnabledForProfile(browser()->profile()));
  EXPECT_FALSE(GetOptimizationGuide()->ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_COMPOSE));

  // Enable Compose and dependent features via the Optimization Guide's pref.
  EXPECT_FALSE(about_flags::IsRestartNeededToCommitChanges());
  browser()->profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::proto::ModelExecutionFeature::
              MODEL_EXECUTION_FEATURE_COMPOSE),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  EXPECT_TRUE(about_flags::IsRestartNeededToCommitChanges());
}

// Checks that after the browser restarts required features are enabled.
IN_PROC_BROWSER_TEST_F(ComposeEnablingBrowserTest, EnableComposeViaSettings) {
  // Confirm that the required feature flags are enabled.
  EXPECT_TRUE(base::FeatureList::IsEnabled(compose::features::kEnableCompose));
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      autofill::features::kAutofillContentEditables));

  EXPECT_EQ(base::ok(),
            GetComposeEnabling().IsEnabledForProfile(browser()->profile()));
  EXPECT_TRUE(GetOptimizationGuide()->ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_COMPOSE));
}
