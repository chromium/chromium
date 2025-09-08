// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_hats.h"

#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::password_manager::PasswordForm;
using ::password_manager::TestPasswordStore;
using ::password_manager::features_util::
    kPasswordChangeBlockingChallengeDetected;
using ::password_manager::features_util::kPasswordChangeBreachedPasswordsCount;
using ::password_manager::features_util::kPasswordChangeRuntime;
using ::password_manager::features_util::kPasswordChangeSavedPasswordsCount;
using ::password_manager::features_util::
    kPasswordChangeSuggestedPasswordsAdoption;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Return;

PasswordForm CreateTestPasswordForm(std::string_view username,
                                    std::string_view password) {
  PasswordForm form;
  form.url = GURL("https://example.com");
  form.signon_realm = form.url.GetWithEmptyPath().spec();
  form.username_value = base::UTF8ToUTF16(username);
  form.password_value = base::UTF8ToUTF16(password);
  return form;
}

class PasswordChangeHatsTest : public testing::Test {
 public:
  PasswordChangeHatsTest()
      : profile_(std::make_unique<TestingProfile>()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {}
  ~PasswordChangeHatsTest() override = default;

  void SetUp() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey)
        .WillRepeatedly(Return(true));
    profile_store_ = CreateAndUseTestPasswordStore(profile_.get());
    account_store_ = CreateAndUseTestAccountPasswordStore(profile_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  Profile* profile() { return profile_.get(); }
  content::WebContents* web_contents() { return web_contents_; }
  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockHatsService> mock_hats_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
};

TEST_F(PasswordChangeHatsTest, ReportsGeneratedPasswordsAdoption) {
  PasswordForm form = CreateTestPasswordForm("user", "password");
  form.type = password_manager::PasswordForm::Type::kGenerated;
  profile_store().AddLogin(form);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeSuccess, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, true),
                      Pair(kPasswordChangeBlockingChallengeDetected, false)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "0"),
                      Pair(kPasswordChangeSavedPasswordsCount, "1"),
                      Pair(kPasswordChangeRuntime, "0")),
          _, _, _, _, _))
      .Times(1);

  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());
  RunUntilIdle();
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeSuccess,
      /*password_change_duration=*/base::TimeDelta(),
      /*blocking_challenge_detected=*/false, web_contents());
}

TEST_F(PasswordChangeHatsTest, ReportsLeakedPasswordsCount) {
  profile_store().AddLogin(CreateTestPasswordForm("user", "password"));

  PasswordForm leaked_form =
      CreateTestPasswordForm("leaked_user", "leaked_pwd");
  leaked_form.password_issues = {{password_manager::InsecureType::kLeaked,
                                  password_manager::InsecurityMetadata()}};
  account_store().AddLogin(leaked_form);
  RunUntilIdle();

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeCanceled, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, false),
                      Pair(kPasswordChangeBlockingChallengeDetected, false)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "1"),
                      Pair(kPasswordChangeSavedPasswordsCount, "2"),
                      Pair(kPasswordChangeRuntime, "0")),
          _, _, _, _, _))
      .Times(1);

  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());
  RunUntilIdle();
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeCanceled,
      /*password_change_duration=*/base::TimeDelta(),
      /*blocking_challenge_detected=*/false, web_contents());
}

TEST_F(PasswordChangeHatsTest, ReportsPasswordChangeRuntime) {
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeError, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, false),
                      Pair(kPasswordChangeBlockingChallengeDetected, false)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "0"),
                      Pair(kPasswordChangeSavedPasswordsCount, "0"),
                      Pair(kPasswordChangeRuntime, "50")),
          _, _, _, _, _))
      .Times(1);

  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());
  RunUntilIdle();
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeError,
      /*password_change_duration=*/base::Milliseconds(50),
      /*blocking_challenge_detected=*/false, web_contents());
}

TEST_F(PasswordChangeHatsTest, ReportsMinusOneForCountsWithoutFetchedData) {
  profile_store().ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendErrorType::kUncategorized);
  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeError, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, false),
                      Pair(kPasswordChangeBlockingChallengeDetected, false)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "-1"),
                      Pair(kPasswordChangeSavedPasswordsCount, "-1"),
                      Pair(kPasswordChangeRuntime, "50")),
          _, _, _, _, _))
      .Times(1);
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeError,
      /*password_change_duration=*/base::Milliseconds(50),
      /*blocking_challenge_detected=*/false, web_contents());
}

TEST_F(PasswordChangeHatsTest, DoesNotReportPasswordChangeRuntimeWhenNullopt) {
  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeDelayed, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, false),
                      Pair(kPasswordChangeBlockingChallengeDetected, false)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "-1"),
                      Pair(kPasswordChangeSavedPasswordsCount, "-1")),
          _, _, _, _, _))
      .Times(1);
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeDelayed,
      /*password_change_duration=*/std::nullopt,
      /*blocking_challenge_detected=*/false, web_contents());
}

TEST_F(PasswordChangeHatsTest, ReportsBlockingChallengeDetected) {
  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeSuccess, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, false),
                      Pair(kPasswordChangeBlockingChallengeDetected, true)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "-1"),
                      Pair(kPasswordChangeSavedPasswordsCount, "-1")),
          _, _, _, _, _))
      .Times(1);
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeSuccess,
      /*password_change_duration=*/std::nullopt,
      /*blocking_challenge_detected=*/true, web_contents());
}

TEST_F(PasswordChangeHatsTest,
       DoesNotReportBlockingChallengeDetectedWhenNullopt) {
  auto password_change_hats = std::make_unique<PasswordChangeHats>(
      mock_hats_service(), &profile_store(), &account_store());

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPasswordChangeDelayed, web_contents(),
          /*timeout_ms=*/0, /*product_specific_bits_data=*/
          ElementsAre(Pair(kPasswordChangeSuggestedPasswordsAdoption, false)),
          /*product_specific_string_data=*/
          ElementsAre(Pair(kPasswordChangeBreachedPasswordsCount, "-1"),
                      Pair(kPasswordChangeSavedPasswordsCount, "-1")),
          _, _, _, _, _))
      .Times(1);
  password_change_hats->MaybeLaunchSurvey(
      kHatsSurveyTriggerPasswordChangeDelayed,
      /*password_change_duration=*/std::nullopt,
      /*blocking_challenge_detected=*/std::nullopt, web_contents());
}

}  // namespace
