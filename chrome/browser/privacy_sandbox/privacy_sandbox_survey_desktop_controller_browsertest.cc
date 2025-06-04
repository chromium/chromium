// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "privacy_sandbox_survey_desktop_controller_factory.h"

using ::testing::_;
using ::testing::Eq;

namespace privacy_sandbox {

class PrivacySandboxSurveyDesktopControllerBrowserTest
    : public InProcessBrowserTest {
 protected:
  PrivacySandboxSurveyDesktopControllerBrowserTest() {
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

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  raw_ptr<MockHatsService, DanglingUntriaged> mock_hats_service_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxSurveyDesktopControllerBrowserTest,
                       SurveyNotLaunchedOnFirstNtp) {
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey(_, _, _, _, _, _, _)).Times(0);

  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSurveyDesktopControllerBrowserTest,
                       SurveyLaunchedOnSecondNtp) {
  // Arrange: Set some prefs to true before triggering survey.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled,
                      false);  // Explicitly default
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);

  // Expect values matching the modified prefs, still not signed in.
  std::map<std::string, bool> expected_psb = {
      {"Topics enabled", true},
      {"Protected audience enabled", false},
      {"Measurement enabled", true},
      {"Signed in", false},
  };
  std::map<std::string, std::string> expected_psd = {
      {"Channel",
       std::string(version_info::GetChannelString(chrome::GetChannel()))}};

  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(GetSentimentSurveyTriggerId(), _, _,
                           Eq(expected_psb), Eq(expected_psd), _, _));

  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  // Navigation to the 2nd instance of a NTP should trigger the sentiment
  // survey.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

}  // namespace privacy_sandbox
