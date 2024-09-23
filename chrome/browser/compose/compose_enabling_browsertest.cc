// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_enabling.h"

#include <vector>

#include "base/feature_list.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/compose/chrome_compose_client.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
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
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class ComposeEnablingBrowserTestBase : public InProcessBrowserTest {
 public:
  ComposeEnablingBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Set the user country to US, a Compose default-enabled country.
    // Note: CHECK that the value was actually set to confirm that it is
    // not leaking from a previous test run.
    CHECK(
        g_browser_process->variations_service()->OverrideStoredPermanentCountry(
            "us"));
  }

  void TearDownOnMainThread() override {
    // Cleanup the country override.
    CHECK(
        g_browser_process->variations_service()->OverrideStoredPermanentCountry(
            ""));
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void EnableComposePreReqs() {
    optimization_guide::EnableSigninAndModelExecutionCapability(
        browser()->profile());

    // Turn on MSBB.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    // Confirm that the required feature flags are enabled by default.
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(compose::features::kEnableCompose));

    // Enable Compose via the Optimization Guide's pref.
    browser()->profile()->GetPrefs()->SetInteger(
        optimization_guide::prefs::GetSettingEnabledPrefName(
            optimization_guide::UserVisibleFeatureKey::kCompose),
        static_cast<int>(
            optimization_guide::prefs::FeatureOptInState::kEnabled));
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ComposeEnablingBrowserTest : public ComposeEnablingBrowserTestBase {
 public:
  ComposeEnablingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            optimization_guide::features::kOptimizationGuideModelExecution,
            optimization_guide::features::internal::kComposeSettingsVisibility,
        },
        /*disabled_features=*/
        {
            optimization_guide::features::internal::kComposeGraduated,
#if BUILDFLAG(IS_CHROMEOS)
            // All of these flags must be disabled for Compose to be enabled on
            // ChromeOS.
            chromeos::features::kFeatureManagementDisableChromeCompose,
            chromeos::features::kOrca,
            chromeos::features::kOrcaDogfood,
#endif  // BUILDFLAG(IS_CHROMEOS)
        });
  }
};

// PRE_ step simulates a browser restart.
IN_PROC_BROWSER_TEST_F(ComposeEnablingBrowserTest,
                       PRE_EnableComposeViaSettings) {
  EnableComposePreReqs();

  // Checks that Compose is immediately enabled.
  EXPECT_EQ(base::ok(), GetComposeEnabling().IsEnabled());
  EXPECT_TRUE(GetOptimizationGuide()->ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::UserVisibleFeatureKey::kCompose));
}

// Checks that after the browser restarts required features are enabled.
IN_PROC_BROWSER_TEST_F(ComposeEnablingBrowserTest, EnableComposeViaSettings) {
  EXPECT_EQ(base::ok(), GetComposeEnabling().IsEnabled());
  EXPECT_TRUE(GetOptimizationGuide()->ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::UserVisibleFeatureKey::kCompose));
}
class GraduatedComposeEnablingBrowserTest
    : public ComposeEnablingBrowserTestBase {
 public:
  GraduatedComposeEnablingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            optimization_guide::features::kOptimizationGuideModelExecution,
            optimization_guide::features::internal::kComposeGraduated,
        },
        /*disabled_features=*/
        {
            optimization_guide::features::internal::kComposeSettingsVisibility,
#if BUILDFLAG(IS_CHROMEOS)
            // All of these flags must be disabled for Compose to be enabled on
            // ChromeOS.
            chromeos::features::kFeatureManagementDisableChromeCompose,
            chromeos::features::kOrca,
            chromeos::features::kOrcaDogfood,
#endif  // BUILDFLAG(IS_CHROMEOS)
        });
  }
};

// Checks that feature is enabled.
IN_PROC_BROWSER_TEST_F(GraduatedComposeEnablingBrowserTest, GraduatedCompose) {
  EnableComposePreReqs();
  EXPECT_EQ(base::ok(), GetComposeEnabling().IsEnabled());
  EXPECT_TRUE(GetOptimizationGuide()->ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::UserVisibleFeatureKey::kCompose));
}

#if BUILDFLAG(IS_CHROMEOS)

// For testing that the feature is disabled by the appropriate feature
// management flag on CrOS.
class ComposeOnChromeOS : public ComposeEnablingBrowserTestBase {
 public:
  ComposeOnChromeOS() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            optimization_guide::features::kOptimizationGuideModelExecution,
            optimization_guide::features::internal::kComposeSettingsVisibility,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementDisableChromeCompose,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{
            optimization_guide::features::internal::kComposeGraduated,
            chromeos::features::kOrca,
            chromeos::features::kOrcaDogfood,
        });
  }
};

// Similar to above, PRE_ step for checking that Compose is disabled on
// non-eligible CrOS devices.
IN_PROC_BROWSER_TEST_F(ComposeOnChromeOS,
                       PRE_ComposeDisabledOnNonEligibleCrOSDevices) {
  EnableComposePreReqs();

  // Checks that Compose is still disabled.
  EXPECT_EQ(base::unexpected(compose::ComposeShowStatus::kDisabledOnChromeOS),
            GetComposeEnabling().IsEnabled());
}

// Checks that Compose is disabled on non-eligible CrOS devices.
IN_PROC_BROWSER_TEST_F(ComposeOnChromeOS,
                       ComposeDisabledOnNonEligibleCrOSDevices) {
  EXPECT_EQ(base::unexpected(compose::ComposeShowStatus::kDisabledOnChromeOS),
            GetComposeEnabling().IsEnabled());
}

// For testing that the graduated feature is disabled by the appropriate feature
// management flag on CrOS.
class GraduatedComposeOnChromeOS : public ComposeEnablingBrowserTestBase {
 public:
  GraduatedComposeOnChromeOS() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            optimization_guide::features::kOptimizationGuideModelExecution,
            optimization_guide::features::internal::kComposeGraduated,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementDisableChromeCompose,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{
            optimization_guide::features::internal::kComposeSettingsVisibility,
            chromeos::features::kOrca,
            chromeos::features::kOrcaDogfood,
        });
  }
};

IN_PROC_BROWSER_TEST_F(GraduatedComposeOnChromeOS,
                       GraduatedComposeDisabledOnNonEligibleCrOSDevices) {
  EnableComposePreReqs();
  // Checks that Compose is disabled.
  EXPECT_EQ(base::unexpected(compose::ComposeShowStatus::kDisabledOnChromeOS),
            GetComposeEnabling().IsEnabled());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class ComposeEnablingWithFencedFramesBrowserTest
    : public ComposeEnablingBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ComposeEnablingBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    https_server()->ServeFilesFromSourceDirectory("content/test/data");

    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(https_server());

    ASSERT_TRUE(https_server()->Start());
  }

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(ComposeEnablingWithFencedFramesBrowserTest,
                       DisabledInFencedFrames) {
  // Only checking the cross-fence functionality, can ignore other enablement
  // requirements.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  GURL main_url(https_server()->GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(a.test{fenced})"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(main_frame);
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_child1 = child_frames[0];

  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = fenced_child1->GetLastCommittedOrigin();
  EXPECT_FALSE(client->GetComposeEnabling().ShouldTriggerContextMenu(
      browser()->profile(), nullptr, fenced_child1, params));
}
