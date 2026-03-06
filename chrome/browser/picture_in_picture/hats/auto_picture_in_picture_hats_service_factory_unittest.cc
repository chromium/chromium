// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service_factory.h"

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"
#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_test_base.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/testing_profile.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutoPictureInPictureHatsServiceFactoryTest
    : public AutoPictureInPictureHatsTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AutoPictureInPictureHatsServiceFactoryTest() = default;

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
                         AutoPictureInPictureHatsServiceFactoryTest,
                         testing::Bool());

TEST_P(AutoPictureInPictureHatsServiceFactoryTest, BuildServiceInstance) {
  if (is_surveys_feature_enabled()) {
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(false))
        .WillOnce(testing::Return(true));
    EXPECT_NE(nullptr,
              AutoPictureInPictureHatsServiceFactory::GetForProfile(profile()));
  } else {
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(testing::_)).Times(0);
    EXPECT_EQ(nullptr,
              AutoPictureInPictureHatsServiceFactory::GetForProfile(profile()));
  }
}

TEST_P(AutoPictureInPictureHatsServiceFactoryTest, ReturnsSameInstance) {
  if (!is_surveys_feature_enabled()) {
    return;
  }

  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(false))
      .WillOnce(testing::Return(true));

  AutoPictureInPictureHatsService* service1 =
      AutoPictureInPictureHatsServiceFactory::GetForProfile(profile());
  AutoPictureInPictureHatsService* service2 =
      AutoPictureInPictureHatsServiceFactory::GetForProfile(profile());
  EXPECT_EQ(service1, service2);
}

TEST_P(AutoPictureInPictureHatsServiceFactoryTest, NoServiceWithoutHats) {
  if (!is_surveys_feature_enabled()) {
    return;
  }

  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(false))
      .WillOnce(testing::Return(false));
  EXPECT_EQ(nullptr,
            AutoPictureInPictureHatsServiceFactory::GetForProfile(profile()));
}

TEST_P(AutoPictureInPictureHatsServiceFactoryTest, NotCreatedForIncognito) {
  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(testing::_)).Times(0);

  TestingProfile* incognito = TestingProfile::Builder().BuildOffTheRecord(
      profile(), Profile::OTRProfileID::PrimaryID());
  EXPECT_EQ(nullptr,
            AutoPictureInPictureHatsServiceFactory::GetForProfile(incognito));
}

TEST_P(AutoPictureInPictureHatsServiceFactoryTest, NotCreatedForGuestProfile) {
  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(testing::_)).Times(0);

  std::unique_ptr<TestingProfile> guest =
      TestingProfile::Builder().SetGuestSession().Build();
  EXPECT_EQ(nullptr,
            AutoPictureInPictureHatsServiceFactory::GetForProfile(guest.get()));
}

TEST_P(AutoPictureInPictureHatsServiceFactoryTest,
       DifferentProfilesHaveDifferentInstances) {
  if (!is_surveys_feature_enabled()) {
    return;
  }

  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(false))
      .WillOnce(testing::Return(true));

  std::unique_ptr<TestingProfile> profile2 = std::make_unique<TestingProfile>();
  auto* mock_hats_service2 = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile2.get(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service2, CanShowAnySurvey(false))
      .WillOnce(testing::Return(true));

  AutoPictureInPictureHatsService* service1 =
      AutoPictureInPictureHatsServiceFactory::GetForProfile(profile());
  AutoPictureInPictureHatsService* service2 =
      AutoPictureInPictureHatsServiceFactory::GetForProfile(profile2.get());
  EXPECT_NE(service1, service2);
}
