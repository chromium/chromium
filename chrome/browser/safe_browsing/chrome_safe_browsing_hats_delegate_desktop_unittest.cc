// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate_desktop.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class MockHatsService : public HatsServiceDesktop {
 public:
  explicit MockHatsService(Profile* profile) : HatsServiceDesktop(profile) {}
  ~MockHatsService() override = default;

  MOCK_METHOD(HatsService::LaunchError,
              LaunchSurvey,
              (const std::string& trigger,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               const SurveyBitsData& product_specific_bits_data,
               const SurveyStringData& product_specific_string_data,
               const std::optional<std::string>& supplied_trigger_id,
               const SurveyOptions& survey_options),
              (override));
};

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context) {
  return std::make_unique<MockHatsService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class ChromeSafeBrowsingHatsDelegateDesktopTest : public testing::Test {
 public:
  ChromeSafeBrowsingHatsDelegateDesktopTest() = default;

  void SetUp() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindRepeating(&BuildMockHatsService)));
    delegate_ =
        std::make_unique<ChromeSafeBrowsingHatsDelegateDesktop>(&profile_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
  std::unique_ptr<ChromeSafeBrowsingHatsDelegateDesktop> delegate_;
};

TEST_F(ChromeSafeBrowsingHatsDelegateDesktopTest, LaunchRedWarningSurvey) {
  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"}};

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerRedWarning, testing::_, testing::_,
                   /*product_specific_bits_data=*/testing::IsEmpty(),
                   /*product_specific_string_data=*/
                   testing::ElementsAre(testing::Pair(
                       safe_browsing::kFlaggedUrl, "http://example.com")),
                   testing::Eq(std::nullopt), testing::_));

  delegate_->LaunchRedWarningSurvey(std::move(product_specific_string_data),
                                    /*product_specific_bits_data=*/{},
                                    /*is_tab_closed=*/false);
}

TEST_F(ChromeSafeBrowsingHatsDelegateDesktopTest,
       LaunchRedWarningSurvey_FiltersDesktopPsd) {
  std::map<std::string, std::string> all_string_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"},
      {safe_browsing::kMainFrameUrl, "http://page.com"},
      {safe_browsing::kReferrerUrl, "http://ref.com"},
      {safe_browsing::kUserActivityWithUrls, "encoded_proto"},
      {safe_browsing::kUserAction, "PROCEED"},
      {safe_browsing::kReportType, "URL_CLIENT_SIDE_PHISHING"},
  };

  std::map<std::string, std::string> expected_desktop_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"},
      {safe_browsing::kMainFrameUrl, "http://page.com"},
      {safe_browsing::kReferrerUrl, "http://ref.com"},
      {safe_browsing::kUserActivityWithUrls, "encoded_proto"},
  };

  std::map<std::string, bool> bits_data = {
      {safe_browsing::kLearnMoreClicked, true},
  };

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerRedWarning, testing::_, testing::_,
                   /*product_specific_bits_data=*/testing::IsEmpty(),
                   /*product_specific_string_data=*/expected_desktop_data,
                   testing::Eq(std::nullopt), testing::_));

  delegate_->LaunchRedWarningSurvey(std::move(all_string_data),
                                    std::move(bits_data),
                                    /*is_tab_closed=*/false);
}

}  // namespace safe_browsing
