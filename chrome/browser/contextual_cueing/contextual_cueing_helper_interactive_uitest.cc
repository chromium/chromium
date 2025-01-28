// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_nudge_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(ENABLE_GLIC)

class FakeGlicNudgeObserver : public GlicNudgeObserver {
 public:
  void OnTriggerGlicNudgeUI(std::string label) override {
    last_nudge_label_ = label;
  }
  std::string last_nudge_label_;
};

class ContextualCueingHelperBrowserTest : public InProcessBrowserTest {
 public:
  ContextualCueingHelperBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Disable feature engagement logic.
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"}}},
         {features::kGlic, {}},
         {features::kTabstripComboButton, {}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualCueingHelperBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpEnabledHints() {
    optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
    cueing_metadata.add_cueing_configurations()->set_cue_label("test label");
    optimization_guide::OptimizationMetadata metadata;
    metadata.SetAnyMetadataForTesting(cueing_metadata);
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(GURL("https://enabled.com/"),
                            optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
                            metadata);
  }

  void EnableSignIn() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  tabs::GlicNudgeController* glic_nudge_controller() {
    return browser()->browser_window_features()->glic_nudge_controller();
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayed) {
  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://enabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("test label", nudge_observer.last_nudge_label_);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelNotDisplayed) {
  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayedForActiveTab) {
  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://enabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("test label", nudge_observer.last_nudge_label_);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ("test label", nudge_observer.last_nudge_label_);

  browser()->tab_strip_model()->ActivateTabAt(2);
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());
}

#endif
