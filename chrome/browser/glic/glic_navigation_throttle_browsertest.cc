// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_navigation_throttle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace glic {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  explicit MockGlicKeyedService(content::BrowserContext* context)
      : GlicKeyedService(
            Profile::FromBrowserContext(context),
            IdentityManagerFactory::GetForProfile(
                Profile::FromBrowserContext(context)),
            g_browser_process->profile_manager(),
            GlicProfileManager::GetInstance(),
            contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
                Profile::FromBrowserContext(context)),
            actor::ActorKeyedServiceFactory::GetActorKeyedService(context)) {}
  MOCK_METHOD(void,
              ShowUiWithConversationID,
              (BrowserWindowInterface*, mojom::InvocationSource, std::string),
              (override));
};

std::unique_ptr<KeyedService> CreateMockGlicKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<MockGlicKeyedService>(context);
}

void NavigateToURL(Browser* browser, const GURL& url) {
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  content::NavigationController::LoadURLParams params(url);
  params.initiator_origin = url::Origin::Create(
      GURL(features::kGlicWebContinuityOriginatingHost.Get()));
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .LoadURLWithParams(params);
  observer.Wait();
}

class GlicNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  GlicNavigationThrottleBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicWebContinuity,
          {{features::kGlicWebContinuityUrl.name,
            "https://example.com/continuity_test/"},
           {features::kGlicWebContinuityOriginatingHost.name,
            "https://example.com/"}}},
         {features::kGlic, {}}},
        {});

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlicNavigationThrottleBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));

    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateMockGlicKeyedService));
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest,
                       InterceptGlicContinueUrlFromGeminiAndOpenGlicUi) {
  base::HistogramTester histogram_tester;
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(
                                 _, mojom::InvocationSource::kNavigationCapture,
                                 std::string("123")))
      .Times(1);

  GURL target_url("https://www.google.com/");
  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=123&targetUrl=" + target_url.spec());

  NavigateToURL(browser(), continue_url);

  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            target_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureEnabled",
      GeminiNavigationCaptureResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest, Metrics_CIDTooLong) {
  base::HistogramTester histogram_tester;
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(_, _, _)).Times(0);

  GURL target_url("https://www.google.com/");
  std::string long_cid(features::kGlicWebContinuityMaxCIDLength.Get() + 1, 'a');
  GURL continue_url(features::kGlicWebContinuityUrl.Get() + "?cid=" + long_cid +
                    "&targetUrl=" + target_url.spec());

  NavigateToURL(browser(), continue_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureEnabled",
      GeminiNavigationCaptureResult::kCIDTooLong, 1);
}

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest,
                       Metrics_TargetUrlTooLong) {
  base::HistogramTester histogram_tester;
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(_, _, _)).Times(0);

  std::string long_target_url(
      features::kGlicWebContinuityMaxTargetUrlLength.Get() + 1, 'a');
  GURL target_url("https://" + long_target_url + ".com/");
  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=123&targetUrl=" + target_url.spec());

  NavigateToURL(browser(), continue_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureEnabled",
      GeminiNavigationCaptureResult::kTargetUrlTooLong, 1);
}

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest, Metrics_InvalidUrl) {
  base::HistogramTester histogram_tester;
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(_, _, _)).Times(0);

  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=123&targetUrl=invalidurl");

  NavigateToURL(browser(), continue_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureEnabled",
      GeminiNavigationCaptureResult::kInvalidUrl, 1);
}

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest,
                       InterceptGlicContinueUrlFromGemini_NonHttpsTargetUrl) {
  base::HistogramTester histogram_tester;
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(_, _, _)).Times(0);

  GURL target_url("http://www.example.com/");
  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=123&targetUrl=" + target_url.spec());

  NavigateToURL(browser(), continue_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureEnabled",
      GeminiNavigationCaptureResult::kNonHttpsScheme, 1);
}

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest,
                       InterceptGlicContinueUrlFromGemini_NoTargetURL) {
  base::HistogramTester histogram_tester;
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(_, _, _)).Times(0);

  GURL continue_url(features::kGlicWebContinuityUrl.Get() + "?cid=123");

  NavigateToURL(browser(), continue_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureEnabled",
      GeminiNavigationCaptureResult::kNoTargetUrl, 1);
}

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest, Incognito) {
  base::HistogramTester histogram_tester;

  GURL target_url("https://www.google.com/");
  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=123&targetUrl=" + target_url.spec());

  Browser* incognito_browser = CreateIncognitoBrowser();

  NavigateToURL(incognito_browser, continue_url);

  EXPECT_EQ(
      incognito_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
      target_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureDisabled",
      GeminiNavigationCaptureResult::kSuccess, 1);
}

class GlicNavigationThrottleBrowserTestWithNoFeatures
    : public InProcessBrowserTest {
 public:
  GlicNavigationThrottleBrowserTestWithNoFeatures() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicWebContinuity,
          {{features::kGlicWebContinuityUrl.name,
            "https://example.com/continuity_test/"},
           {features::kGlicWebContinuityOriginatingHost.name,
            "https://example.com/"}}}},
        {features::kGlic});

    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTestWithNoFeatures,
                       InterceptGlicContinueUrlFromGemini) {
  base::HistogramTester histogram_tester;
  GURL target_url("https://www.google.com/");
  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=123&targetUrl=" + target_url.spec());

  NavigateToURL(browser(), continue_url);

  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            target_url);

  histogram_tester.ExpectUniqueSample(
      "Glic.NavigationCapture.GlicWebContinuityFeatureDisabled",
      GeminiNavigationCaptureResult::kSuccess, 1);
}

class GlicNavigationThrottleBrowserTestWithPref : public InProcessBrowserTest {
 public:
  GlicNavigationThrottleBrowserTestWithPref() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicWebContinuity,
          {{features::kGlicWebContinuityUrl.name,
            "https://example.com/continuity_test/"}}},
         {features::kGlic, {}}},
        {});

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlicNavigationThrottleBrowserTestWithPref::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));

    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateMockGlicKeyedService));
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTestWithPref,
                       FallbackToPref) {
  const std::string pref_url = "https://pref.example.com/";
  g_browser_process->local_state()->SetString(
      prefs::kGlicWebContinuityOriginatingHostUrlPreset, pref_url);

  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(
                                 _, mojom::InvocationSource::kNavigationCapture,
                                 std::string("456")))
      .Times(1);

  GURL target_url("https://www.google.com/");
  GURL continue_url(features::kGlicWebContinuityUrl.Get() +
                    "?cid=456&targetUrl=" + target_url.spec());

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::NavigationController::LoadURLParams params(continue_url);
  params.initiator_origin = url::Origin::Create(GURL(pref_url));
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .LoadURLWithParams(params);
  observer.Wait();

  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            target_url);

  // Clear the pref to avoid affecting other tests.
  g_browser_process->local_state()->ClearPref(
      prefs::kGlicWebContinuityOriginatingHostUrlPreset);
}

}  // namespace glic
