// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/chrome_safe_browsing_hats_delegate_android.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/android/hats/hats_service_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {

class MockHatsService : public HatsServiceAndroid {
 public:
  explicit MockHatsService(Profile* profile) : HatsServiceAndroid(profile) {}
  ~MockHatsService() override = default;

  MOCK_METHOD(HatsService::LaunchError,
              LaunchSurveyForWebContents,
              (const std::string& trigger,
               content::WebContents* web_contents,
               const SurveyBitsData& product_specific_bits_data,
               const SurveyStringData& product_specific_string_data,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               const std::optional<std::string>& supplied_trigger_id,
               const SurveyOptions& survey_options),
              (override));
};

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context) {
  return std::make_unique<MockHatsService>(
      Profile::FromBrowserContext(context));
}

class ScopedTabModelRegistration {
 public:
  explicit ScopedTabModelRegistration(TestTabModel* model) : model_(model) {
    CHECK(model_);
    TabModelList::AddTabModel(model_);
  }
  ~ScopedTabModelRegistration() { TabModelList::RemoveTabModel(model_); }

  ScopedTabModelRegistration(const ScopedTabModelRegistration&) = delete;
  ScopedTabModelRegistration& operator=(const ScopedTabModelRegistration&) =
      delete;

 private:
  raw_ptr<TestTabModel> model_ = nullptr;
};

}  // namespace

class ChromeSafeBrowsingHatsDelegateAndroidTest : public testing::Test {
 public:
  ChromeSafeBrowsingHatsDelegateAndroidTest() {
    feature_list_.InitAndEnableFeature(safe_browsing::kRedWarningSurveyAndroid);
  }

  void SetUp() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindRepeating(&BuildMockHatsService)));
    delegate_ =
        std::make_unique<ChromeSafeBrowsingHatsDelegateAndroid>(&profile_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
  std::unique_ptr<ChromeSafeBrowsingHatsDelegateAndroid> delegate_;
};

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest, LaunchRedWarningSurvey) {
  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"}};

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  TestTabModel tab_model(&profile_);
  tab_model.SetWebContentsList({web_contents.get()});
  ScopedTabModelRegistration tab_model_registration(&tab_model);

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerRedWarningAndroid, web_contents.get(), testing::_,
          testing::_, testing::_, testing::_, testing::Eq(std::nullopt),
          testing::Field(&HatsService::SurveyOptions::custom_invitation,
                         testing::Eq(l10n_util::GetStringUTF16(
                             IDS_SAFE_BROWSING_HATS_CUSTOM_INVITATION)))));

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{});
}

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest,
       LaunchRedWarningSurvey_PassesBitsData) {
  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kUserAction, safe_browsing::kUserActionProceed}};
  std::map<std::string, bool> product_specific_bits_data = {
      {safe_browsing::kLearnMoreClicked, true},
      {safe_browsing::kShowMoreClicked, true},
  };

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  TestTabModel tab_model(&profile_);
  tab_model.SetWebContentsList({web_contents.get()});
  ScopedTabModelRegistration tab_model_registration(&tab_model);

  EXPECT_CALL(*mock_hats_service_,
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerRedWarningAndroid, web_contents.get(),
                  product_specific_bits_data, testing::_, testing::_,
                  testing::_, testing::Eq(std::nullopt), testing::_));

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    std::move(product_specific_bits_data));
}

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest,
       LaunchRedWarningSurvey_IncognitoNoSurvey) {
  std::map<std::string, std::string> product_specific_string_data = {
      {"test_key", "test_value"}};

  Profile* incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          incognito_profile, content::SiteInstance::Create(incognito_profile));

  TestTabModel tab_model(incognito_profile);
  tab_model.SetWebContentsList({incognito_web_contents.get()});
  ScopedTabModelRegistration tab_model_registration(&tab_model);

  EXPECT_CALL(*mock_hats_service_, LaunchSurveyForWebContents).Times(0);

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{});
}

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest,
       LaunchRedWarningSurvey_EmptyTabModelListNoSurvey) {
  std::map<std::string, std::string> product_specific_string_data = {
      {"test_key", "test_value"}};

  EXPECT_CALL(*mock_hats_service_, LaunchSurveyForWebContents).Times(0);

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{});
}

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest,
       LaunchRedWarningSurvey_MasterTriggerIdOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kRedWarningSurveyAndroid,
      {{"RedWarningSurveyAndroidTriggerId", "custom_override_id"}});

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  TestTabModel tab_model(&profile_);
  tab_model.SetWebContentsList({web_contents.get()});
  ScopedTabModelRegistration tab_model_registration(&tab_model);

  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kUserAction, safe_browsing::kUserActionProceed}};

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerRedWarningAndroid, web_contents.get(), testing::_,
          testing::_, testing::_, testing::_,
          testing::Eq(std::make_optional(std::string("custom_override_id"))),
          testing::_));

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{});
}

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest,
       LaunchRedWarningSurvey_ProceedTriggerIdOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kRedWarningSurveyAndroid,
      {{"RedWarningSurveyAndroidProceedTriggerId", "custom_proceed_id"}});

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  TestTabModel tab_model(&profile_);
  tab_model.SetWebContentsList({web_contents.get()});
  ScopedTabModelRegistration tab_model_registration(&tab_model);

  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kUserAction, safe_browsing::kUserActionProceed}};

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerRedWarningAndroid, web_contents.get(), testing::_,
          testing::_, testing::_, testing::_,
          testing::Eq(std::make_optional(std::string("custom_proceed_id"))),
          testing::_));

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{});
}

TEST_F(ChromeSafeBrowsingHatsDelegateAndroidTest,
       LaunchRedWarningSurvey_HeedTriggerIdOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kRedWarningSurveyAndroid,
      {{"RedWarningSurveyAndroidHeedTriggerId", "custom_heed_id"}});

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  TestTabModel tab_model(&profile_);
  tab_model.SetWebContentsList({web_contents.get()});
  ScopedTabModelRegistration tab_model_registration(&tab_model);

  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kUserAction, safe_browsing::kUserActionDontProceed}};

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerRedWarningAndroid, web_contents.get(), testing::_,
          testing::_, testing::_, testing::_,
          testing::Eq(std::make_optional(std::string("custom_heed_id"))),
          testing::_));

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{});
}

}  // namespace safe_browsing
