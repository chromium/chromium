// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "privacy_sandbox_survey_desktop_controller_factory.h"
#include "privacy_sandbox_survey_factory.h"

using ::testing::_;

namespace privacy_sandbox {

class PrivacySandboxSurveyDesktopControllerTest : public InProcessBrowserTest {
 protected:
  PrivacySandboxSurveyDesktopControllerTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

  void SetUpOnMainThread() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service_, CanShowAnySurvey(testing::_))
        .WillRepeatedly(testing::Return(true));

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kPrivacySandboxSentimentSurvey,
             {{"sentiment-survey-trigger-id", "trigger-id"}}}};
  }

  std::string GetSentimentSurveyTriggerId() {
    return kHatsSurveyTriggerPrivacySandboxSentimentSurvey;
  }

  PrivacySandboxSurveyDesktopController* survey_desktop_controller() {
    return PrivacySandboxSurveyDesktopControllerFactory::GetForProfile(
        browser()->profile());
  }

  PrivacySandboxSurveyService* survey_service() {
    return PrivacySandboxSurveyFactory::GetForProfile(browser()->profile());
  }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  raw_ptr<MockHatsService, DanglingUntriaged> mock_hats_service_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

class PrivacySandboxSurveyDesktopControllerLaunchSurveyTest
    : public PrivacySandboxSurveyDesktopControllerTest {
 protected:
  void OnSentimentSurveyShown() {
    survey_desktop_controller()->OnSentimentSurveyShown(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxSurveyDesktopControllerLaunchSurveyTest,
                       SurveyNotLaunchedOnFirstNtp) {
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(GetSentimentSurveyTriggerId(), _, _, _, _))
      .Times(0);

  // Navigation to the first NTP should not trigger the sentiment survey.
  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSurveyDesktopControllerLaunchSurveyTest,
                       SurveyLaunchedOnSecondNtp) {
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(GetSentimentSurveyTriggerId(), _, _,
                           survey_service()->GetSentimentSurveyPsb(), _));

  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  // Navigation to the 2nd instance of a NTP should trigger the sentiment
  // survey.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSurveyDesktopControllerLaunchSurveyTest,
                       SurveyMarkedAsSeen) {
  EXPECT_TRUE(
      prefs()
          ->FindPreference(prefs::kPrivacySandboxSentimentSurveyLastSeen)
          ->IsDefaultValue());

  // Simulate a call to the successful callback
  OnSentimentSurveyShown();

  // Expect that the survey was marked as seen.
  EXPECT_FALSE(
      prefs()
          ->FindPreference(prefs::kPrivacySandboxSentimentSurveyLastSeen)
          ->IsDefaultValue());
}

}  // namespace privacy_sandbox
