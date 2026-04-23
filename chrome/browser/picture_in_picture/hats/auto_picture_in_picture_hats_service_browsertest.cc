// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::PictureInPictureEventsInfo;
using PromptResult = AutoPipSettingHelper::PromptResult;
using testing::_;

class AutoPictureInPictureHatsBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&AutoPictureInPictureHatsBrowserTestBase::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockHatsService));
  }

  MockHatsService* GetMockHatsService(Profile* profile) {
    return static_cast<MockHatsService*>(
        HatsServiceFactory::GetForProfile(profile,
                                          /*create_if_necessary=*/true));
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{media::kAutoPictureInPictureSurveys, {}}};
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

class AutoPictureInPictureHatsBrowserTest
    : public AutoPictureInPictureHatsBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  bool is_surveys_feature_enabled() const { return GetParam(); }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return is_surveys_feature_enabled()
               ? std::vector<
                     base::test::
                         FeatureRefAndParams>{{media::
                                                   kAutoPictureInPictureSurveys,
                                               {}}}
               : std::vector<base::test::FeatureRefAndParams>{};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return is_surveys_feature_enabled()
               ? std::vector<base::test::FeatureRef>{}
               : std::vector<base::test::FeatureRef>{
                     media::kAutoPictureInPictureSurveys};
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutoPictureInPictureHatsBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(AutoPictureInPictureHatsBrowserTest,
                       ServiceCreationRespectsFeatureFlag) {
  Profile* profile = browser()->profile();
  if (is_surveys_feature_enabled()) {
    ON_CALL(*GetMockHatsService(profile), CanShowAnySurvey(false))
        .WillByDefault(testing::Return(true));

    // Verify that the AutoPictureInPictureHatsService was registered and
    // is reachable via its factory.
    EXPECT_NE(nullptr,
              AutoPictureInPictureHatsServiceFactory::GetForProfile(profile));
  } else {
    EXPECT_EQ(nullptr,
              AutoPictureInPictureHatsServiceFactory::GetForProfile(profile));
  }
}

class AutoPictureInPictureHatsEnabledBrowserTest
    : public AutoPictureInPictureHatsBrowserTestBase {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{media::kAutoPictureInPictureSurveys,
             {{"autopip_reason", "VideoConferencing"},
              {"prompt_result", "AllowOnce"}}}};
  }
};

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureHatsEnabledBrowserTest,
                       TriggersSurveyOnWindowClose) {
  Profile* profile = browser()->profile();
  auto* hats_service = GetMockHatsService(profile);
  ON_CALL(*hats_service, CanShowAnySurvey(false))
      .WillByDefault(testing::Return(true));

  auto* autopip_hats_service =
      AutoPictureInPictureHatsServiceFactory::GetForProfile(profile);
  ASSERT_NE(nullptr, autopip_hats_service);

  autopip_hats_service->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  autopip_hats_service->SetPromptResult(PromptResult::kAllowOnce);
  autopip_hats_service->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*hats_service,
              LaunchSurveyForWebContents(kHatsSurveyTriggerAutoPipAllowed, _, _,
                                         _, _, _, _, _));

  autopip_hats_service->MaybeLaunchSurvey(
      browser()->tab_strip_model()->GetActiveWebContents());
}
