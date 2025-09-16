// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using profile_ref = base::optional_ref<const AutofillProfile>;
using ::testing::AllOf;
using ::testing::Property;

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class MockSaveUpdateAddressProfilePromptView
    : public SaveUpdateAddressProfilePromptView {
 public:
  MOCK_METHOD(bool,
              Show,
              (SaveUpdateAddressProfilePromptController * controller,
               const AutofillProfile& autofill_profile,
               SaveUpdateAddressProfilePromptMode prompt_mode),
              (override));
};

class SaveUpdateAddressProfilePromptControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  static constexpr char const* kUserEmail = "example@gmail.com";

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_personal_data_.SetSyncServiceForTest(&sync_service_);

    profile_ = test::GetFullProfile();
    original_profile_ = test::GetFullProfile();
    original_profile_.SetInfo(NAME_FULL, u"John Doe", GetLocale());
    original_profile_.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"", GetLocale());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateTestSyncService)}};
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  // Profile with verified data as it is returned from Java.
  AutofillProfile GetFullProfileWithVerifiedData() {
    AutofillProfile profile(AddressCountryCode("US"));
    profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Mona J. Liza",
                                             VerificationStatus::kUserVerified);
    test::SetProfileInfo(&profile, "", "", "", "email@example.com",
                         "Company Inc.", "33 Narrow Street", "Apt 42",
                         "Playa Vista", "LA", "12345", "US", "13105551234",
                         /*finalize=*/true, VerificationStatus::kUserVerified);
    return profile;
  }

 protected:
  void SigninUser();
  void SetUpController(SaveUpdateAddressProfilePromptMode prompt_mode);

  std::string GetLocale() { return "en-US"; }

  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  autofill::TestPersonalDataManager test_personal_data_;
  raw_ptr<MockSaveUpdateAddressProfilePromptView> prompt_view_ = nullptr;
  AutofillProfile profile_{
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode};
  AutofillProfile original_profile_{
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode};
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      decision_callback_;
  base::MockCallback<base::OnceCallback<void()>> dismissal_callback_;
  std::unique_ptr<SaveUpdateAddressProfilePromptController> controller_;
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  base::android::JavaParamRef<jobject> mock_caller_{nullptr};

 private:
  base::test::ScopedFeatureList feature_{
      features::kAutofillEnableSupportForHomeAndWork};
};

void SaveUpdateAddressProfilePromptControllerTest::SigninUser() {
  identity_test_env_.MakePrimaryAccountAvailable(kUserEmail,
                                                 signin::ConsentLevel::kSignin);
}

void SaveUpdateAddressProfilePromptControllerTest::SetUpController(
    SaveUpdateAddressProfilePromptMode prompt_mode) {
  auto prompt_view = std::make_unique<MockSaveUpdateAddressProfilePromptView>();
  prompt_view_ = prompt_view.get();
  controller_ = std::make_unique<SaveUpdateAddressProfilePromptController>(
      std::move(prompt_view), &test_personal_data_, profile_,
      prompt_mode == SaveUpdateAddressProfilePromptMode::kUpdateProfile
          ? &original_profile_
          : nullptr,
      prompt_mode, decision_callback_.Get(), dismissal_callback_.Get());
  ON_CALL(*prompt_view_, Show(controller_.get(), profile_, prompt_mode))
      .WillByDefault(testing::Return(true));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPromptWhenSave) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  EXPECT_CALL(*prompt_view_,
              Show(controller_.get(), profile_,
                   SaveUpdateAddressProfilePromptMode::kSaveNewProfile));
  controller_->DisplayPrompt();
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPromptWhenMigrate) {
  SigninUser();
  SetUpController(SaveUpdateAddressProfilePromptMode::kMigrateProfile);

  EXPECT_CALL(*prompt_view_,
              Show(controller_.get(), profile_,
                   SaveUpdateAddressProfilePromptMode::kMigrateProfile));
  controller_->DisplayPrompt();
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPromptWhenUpdate) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);
  EXPECT_CALL(*prompt_view_,
              Show(controller_.get(), profile_,
                   SaveUpdateAddressProfilePromptMode::kUpdateProfile));
  controller_->DisplayPrompt();
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenShowReturnsFalse) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  EXPECT_CALL(*prompt_view_,
              Show(controller_.get(), profile_,
                   SaveUpdateAddressProfilePromptMode::kSaveNewProfile))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->DisplayPrompt();
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserAccepts) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  controller_->DisplayPrompt();

  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  controller_->OnUserAccepted(env_);
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserDeclines) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  controller_->DisplayPrompt();

  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined,
                  Property(&profile_ref::has_value, false)));
  controller_->OnUserDeclined(env_);
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserDeclinesMigration) {
  SigninUser();
  SetUpController(SaveUpdateAddressProfilePromptMode::kMigrateProfile);
  controller_->DisplayPrompt();

  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kNever,
                  Property(&profile_ref::has_value, false)));
  controller_->OnUserDeclined(env_);
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserEditsProfile) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  controller_->DisplayPrompt();

  AutofillProfile edited_profile = GetFullProfileWithVerifiedData();
  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                  AllOf(Property(&profile_ref::has_value, true),
                        Property(&profile_ref::value, edited_profile))));
  base::android::ScopedJavaLocalRef<jobject> edited_profile_java =
      edited_profile.CreateJavaObject(
          g_browser_process->GetApplicationLocale());

  controller_->OnUserEdited(env_, edited_profile_java);
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenPromptIsDismissed) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  controller_->DisplayPrompt();

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->OnPromptDismissed(env_);
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenControllerDiesWithoutInteraction) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  controller_->DisplayPrompt();

  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                  Property(&profile_ref::has_value, false)));
  controller_.reset();
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenSaveLocalOrSyncAddress) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());

  EXPECT_EQ(
      u"John H. Doe\nUnderworld\n666 Erebus St.\nApt 8\nElysium, CA "
      u"91111\nUnited States",
      controller_->GetAddress());

  EXPECT_EQ(u"johndoe@hades.com", controller_->GetEmail());
  EXPECT_EQ(u"16502111111", controller_->GetPhoneNumber());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  EXPECT_EQ(u"", controller_->GetRecordTypeNotice(
                     identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenMigrateLocalAddress) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  SigninUser();
  SetUpController(SaveUpdateAddressProfilePromptMode::kMigrateProfile);

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_MIGRATION_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());

  EXPECT_EQ(u"John H. Doe\n666 Erebus St.", controller_->GetAddress());

  EXPECT_EQ(u"johndoe@hades.com", controller_->GetEmail());
  EXPECT_EQ(u"16502111111", controller_->GetPhoneNumber());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_MIGRATE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE,
          base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenMigrateSyncAddress) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill});
  SigninUser();
  SetUpController(SaveUpdateAddressProfilePromptMode::kMigrateProfile);

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_MIGRATION_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());

  EXPECT_EQ(u"John H. Doe\n666 Erebus St.", controller_->GetAddress());

  EXPECT_EQ(u"johndoe@hades.com", controller_->GetEmail());
  EXPECT_EQ(u"16502111111", controller_->GetPhoneNumber());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_MIGRATE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SYNCABLE_PROFILE_MIGRATION_PROMPT_NOTICE,
          base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenSaveAccountAddress) {
  SigninUser();
  test_api(profile_).set_record_type(AutofillProfile::RecordType::kAccount);
  SetUpController(SaveUpdateAddressProfilePromptMode::kSaveNewProfile);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());

  EXPECT_EQ(
      u"John H. Doe\nUnderworld\n666 Erebus St.\nApt 8\nElysium, CA "
      u"91111\nUnited States",
      controller_->GetAddress());

  EXPECT_EQ(u"johndoe@hades.com", controller_->GetEmail());
  EXPECT_EQ(u"16502111111", controller_->GetPhoneNumber());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_ADDRESS_WILL_BE_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE,
          base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenUpdateLocalOrSyncAddress) {
  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(u"John Doe, 666 Erebus St.", controller_->GetSubtitle());
  EXPECT_EQ(u"John Doe", controller_->GetOldDiff());
  EXPECT_EQ(u"John H. Doe\n16502111111", controller_->GetNewDiff());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  EXPECT_EQ(u"", controller_->GetRecordTypeNotice(
                     identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenUpdateAccountAddress) {
  SigninUser();
  test_api(original_profile_)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  test_api(profile_).set_record_type(AutofillProfile::RecordType::kAccount);

  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(u"John Doe, 666 Erebus St.", controller_->GetSubtitle());
  EXPECT_EQ(u"John Doe", controller_->GetOldDiff());
  EXPECT_EQ(u"John H. Doe\n16502111111", controller_->GetNewDiff());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_ADDRESS_ALREADY_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE,
          base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenUpdateWithAddressChanged) {
  original_profile_ = test::GetFullProfile();
  original_profile_.SetInfo(ADDRESS_HOME_ZIP, u"", GetLocale());
  original_profile_.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"", GetLocale());
  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);

  // Subtitle should contain the full name only.
  EXPECT_EQ(u"John H. Doe", controller_->GetSubtitle());
  // Differences should contain envelope style address.
  EXPECT_EQ(u"Underworld\n666 Erebus St.\nApt 8\nElysium, CA \nUnited States",
            controller_->GetOldDiff());
  // There should be an extra newline between address and contacts data.
  EXPECT_EQ(
      u"Underworld\n666 Erebus St.\nApt 8\nElysium, CA 91111\nUnited "
      u"States\n\n16502111111",
      controller_->GetNewDiff());
  EXPECT_EQ(u"", controller_->GetRecordTypeNotice(
                     identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenNewInfoIsAddedToAccount) {
  SigninUser();
  original_profile_ = test::GetFullProfile();
  test_api(original_profile_)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  original_profile_.SetInfo(EMAIL_ADDRESS, u"", GetLocale());

  profile_ = test::GetFullProfile();
  test_api(profile_).set_record_type(AutofillProfile::RecordType::kAccount);
  profile_.SetInfo(EMAIL_ADDRESS, u"a@b.com", GetLocale());

  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_INFO_ADDRESS_PROMPT_TITLE),
      controller_->GetTitle());
  EXPECT_EQ(u"John H. Doe, 666 Erebus St.", controller_->GetSubtitle());
  EXPECT_EQ(u"", controller_->GetOldDiff());
  EXPECT_EQ(u"a@b.com", controller_->GetNewDiff());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_UPDATE_ADDRESS_ADD_NEW_INFO_PROMPT_OK_BUTTON_LABEL),
      controller_->GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_ADDRESS_ALREADY_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE,
          base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenNewInfoIsAddedToAccountHome) {
  SigninUser();
  original_profile_ = test::GetFullProfile();
  test_api(original_profile_)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  original_profile_.SetInfo(EMAIL_ADDRESS, u"", GetLocale());

  profile_ = test::GetFullProfile();
  test_api(profile_).set_record_type(AutofillProfile::RecordType::kAccount);
  profile_.SetInfo(EMAIL_ADDRESS, u"a@b.com", GetLocale());

  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_WITH_MORE_INFO_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(u"John H. Doe, 666 Erebus St.", controller_->GetSubtitle());
  EXPECT_EQ(u"", controller_->GetOldDiff());
  EXPECT_EQ(u"a@b.com", controller_->GetNewDiff());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_ADDRESS_HOME_RECORD_TYPE_NOTICE,
                                 base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

TEST_F(SaveUpdateAddressProfilePromptControllerTest,
       ReturnsCorrectStringsToDisplayWhenNewInfoIsAddedToAccountWork) {
  SigninUser();
  original_profile_ = test::GetFullProfile();
  test_api(original_profile_)
      .set_record_type(AutofillProfile::RecordType::kAccountWork);
  original_profile_.SetInfo(EMAIL_ADDRESS, u"", GetLocale());

  profile_ = test::GetFullProfile();
  test_api(profile_).set_record_type(AutofillProfile::RecordType::kAccount);
  profile_.SetInfo(EMAIL_ADDRESS, u"a@b.com", GetLocale());

  SetUpController(SaveUpdateAddressProfilePromptMode::kUpdateProfile);

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_WITH_MORE_INFO_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(u"John H. Doe, 666 Erebus St.", controller_->GetSubtitle());
  EXPECT_EQ(u"", controller_->GetOldDiff());
  EXPECT_EQ(u"a@b.com", controller_->GetNewDiff());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_ADDRESS_WORK_RECORD_TYPE_NOTICE,
                                 base::ASCIIToUTF16(kUserEmail)),
      controller_->GetRecordTypeNotice(identity_test_env_.identity_manager()));
}

}  // namespace
}  // namespace autofill
