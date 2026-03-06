// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    return {media::kAutoPictureInPictureSurveys};
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(GetEnabledFeatures(), GetDisabledFeatures());
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

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    return is_surveys_feature_enabled()
               ? std::vector<
                     base::test::FeatureRef>{media::
                                                 kAutoPictureInPictureSurveys}
               : std::vector<base::test::FeatureRef>{};
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
