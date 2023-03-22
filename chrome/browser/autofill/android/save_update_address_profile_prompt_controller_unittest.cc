// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class MockSaveUpdateAddressProfilePromptView
    : public SaveUpdateAddressProfilePromptView {
 public:
  MOCK_METHOD(bool,
              Show,
              (SaveUpdateAddressProfilePromptController * controller,
               const AutofillProfile& autofill_profile,
               bool is_update),
              (override));
};

class SaveUpdateAddressProfilePromptControllerTest
    : public ChromeRenderViewHostTestHarness,
      public ::testing::WithParamInterface<
          std::tuple<AutofillProfile::Source, bool>> {
 public:
  static constexpr char const* kUserEmail = "example@gmail.com";

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    profile_ = test::GetFullProfile();
    profile_.set_source_for_testing(profile_source());
    original_profile_ = test::GetFullProfile();
    original_profile_.set_source_for_testing(profile_source());
    original_profile_.SetInfo(NAME_FULL, u"John Doe", GetLocale());
    original_profile_.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"", GetLocale());
    if (profile_source() == AutofillProfile::Source::kAccount ||
        is_migration_to_account()) {
      identity_test_env_.MakePrimaryAccountAvailable(
          kUserEmail, signin::ConsentLevel::kSignin);
    }
    SetUpController(/*is_update=*/false);

    CountryNames::SetLocaleString(GetLocale());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  // Profile with verified data as it is returned from Java.
  AutofillProfile GetFullProfileWithVerifiedData() {
    AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
    profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Mona J. Liza",
                                             VerificationStatus::kUserVerified);
    test::SetProfileInfo(&profile, "", "", "", "email@example.com",
                         "Company Inc.", "33 Narrow Street", "Apt 42",
                         "Playa Vista", "LA", "12345", "US", "13105551234",
                         /*finalize=*/true, VerificationStatus::kUserVerified);
    return profile;
  }

  AutofillProfile::Source profile_source() const {
    return std::get<0>(GetParam());
  }

  bool is_migration_to_account() const { return std::get<1>(GetParam()); }

 protected:
  bool ShouldShowFooter() const;
  void SetUpController(bool is_update);

  std::string GetLocale() { return "en-US"; }

  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<MockSaveUpdateAddressProfilePromptView> prompt_view_;
  AutofillProfile profile_;
  AutofillProfile original_profile_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      decision_callback_;
  base::MockCallback<base::OnceCallback<void()>> dismissal_callback_;
  std::unique_ptr<SaveUpdateAddressProfilePromptController> controller_;
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  base::android::JavaParamRef<jobject> mock_caller_{nullptr};
};

bool SaveUpdateAddressProfilePromptControllerTest::ShouldShowFooter() const {
  return is_migration_to_account() ||
         profile_source() == AutofillProfile::Source::kAccount;
}

void SaveUpdateAddressProfilePromptControllerTest::SetUpController(
    bool is_update) {
  auto prompt_view = std::make_unique<MockSaveUpdateAddressProfilePromptView>();
  prompt_view_ = prompt_view.get();
  controller_ = std::make_unique<SaveUpdateAddressProfilePromptController>(
      std::move(prompt_view), profile_,
      is_update ? &original_profile_ : nullptr, is_migration_to_account(),
      decision_callback_.Get(), dismissal_callback_.Get());
  ON_CALL(*prompt_view_, Show(controller_.get(), profile_, is_update))
      .WillByDefault(testing::Return(true));
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPromptWhenSave) {
  EXPECT_CALL(*prompt_view_, Show(controller_.get(), profile_, false));
  controller_->DisplayPrompt();
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPromptWhenUpdate) {
  SetUpController(/*is_update=*/true);
  EXPECT_CALL(*prompt_view_, Show(controller_.get(), profile_, true));
  controller_->DisplayPrompt();
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenShowReturnsFalse) {
  EXPECT_CALL(*prompt_view_, Show(controller_.get(), profile_, false))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->DisplayPrompt();
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserAccepts) {
  controller_->DisplayPrompt();

  EXPECT_CALL(
      decision_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile_));
  controller_->OnUserAccepted(env_, mock_caller_);
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserDeclines) {
  controller_->DisplayPrompt();

  EXPECT_CALL(
      decision_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          profile_));
  controller_->OnUserDeclined(env_, mock_caller_);
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserEditsProfile) {
  controller_->DisplayPrompt();

  AutofillProfile edited_profile = GetFullProfileWithVerifiedData();
  EXPECT_CALL(
      decision_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
          edited_profile));
  base::android::ScopedJavaLocalRef<jobject> edited_profile_java =
      PersonalDataManagerAndroid::CreateJavaProfileFromNative(env_,
                                                              edited_profile);
  controller_->OnUserEdited(
      env_, mock_caller_,
      base::android::JavaParamRef<jobject>(env_, edited_profile_java.obj()));
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenPromptIsDismissed) {
  controller_->DisplayPrompt();

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->OnPromptDismissed(env_, mock_caller_);
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenControllerDiesWithoutInteraction) {
  controller_->DisplayPrompt();

  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));
  controller_.reset();
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldReturnDataToDisplayWhenSave) {
  if (is_migration_to_account()) {
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_SAVE_ADDRESS_MIGRATION_PROMPT_TITLE),
              controller_->GetTitle());
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_MIGRATION_OK_BUTTON_LABEL),
              controller_->GetPositiveButtonText());
  } else {
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
              controller_->GetTitle());
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
              controller_->GetPositiveButtonText());
  }

  EXPECT_EQ(
      u"John H. Doe\nUnderworld\n666 Erebus St.\nApt 8\nElysium, CA "
      u"91111\nUnited States",
      controller_->GetAddress());
  EXPECT_EQ(u"johndoe@hades.com", controller_->GetEmail());
  EXPECT_EQ(u"16502111111", controller_->GetPhoneNumber());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  if (ShouldShowFooter()) {
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
            base::ASCIIToUTF16(kUserEmail)),
        controller_->GetSourceNotice(identity_test_env_.identity_manager()));
  } else {
    EXPECT_EQ(u"", controller_->GetSourceNotice(
                       identity_test_env_.identity_manager()));
  }
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldReturnDataToDisplayWhenUpdate) {
  SetUpController(/*is_update=*/true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
            controller_->GetTitle());
  EXPECT_EQ(u"John Doe, 666 Erebus St.", controller_->GetSubtitle());
  std::pair<std::u16string, std::u16string> differences =
      controller_->GetDiffFromOldToNewProfile();
  EXPECT_EQ(u"John Doe", differences.first);
  EXPECT_EQ(u"John H. Doe\n16502111111", differences.second);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            controller_->GetPositiveButtonText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL),
            controller_->GetNegativeButtonText());

  if (ShouldShowFooter()) {
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
            base::ASCIIToUTF16(kUserEmail)),
        controller_->GetSourceNotice(identity_test_env_.identity_manager()));
  } else {
    EXPECT_EQ(u"", controller_->GetSourceNotice(
                       identity_test_env_.identity_manager()));
  }
}

TEST_P(SaveUpdateAddressProfilePromptControllerTest,
       ShouldReturnDataToDisplayWhenUpdateWithAddressChanged) {
  original_profile_ = test::GetFullProfile();
  original_profile_.SetInfo(ADDRESS_HOME_ZIP, u"", GetLocale());
  original_profile_.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"", GetLocale());
  SetUpController(/*is_update=*/true);

  // Subtitle should contain the full name only.
  EXPECT_EQ(u"John H. Doe", controller_->GetSubtitle());
  std::pair<std::u16string, std::u16string> differences =
      controller_->GetDiffFromOldToNewProfile();
  // Differences should contain envelope style address.
  EXPECT_EQ(u"Underworld\n666 Erebus St.\nApt 8\nElysium, CA \nUnited States",
            differences.first);
  // There should be an extra newline between address and contacts data.
  EXPECT_EQ(
      u"Underworld\n666 Erebus St.\nApt 8\nElysium, CA 91111\nUnited "
      u"States\n\n16502111111",
      differences.second);
  if (ShouldShowFooter()) {
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
            base::ASCIIToUTF16(kUserEmail)),
        controller_->GetSourceNotice(identity_test_env_.identity_manager()));
  } else {
    EXPECT_EQ(u"", controller_->GetSourceNotice(
                       identity_test_env_.identity_manager()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    SaveUpdateAddressProfilePrompt,
    SaveUpdateAddressProfilePromptControllerTest,
    ::testing::ValuesIn(
        {std::tuple(AutofillProfile::Source::kLocalOrSyncable, false),
         std::tuple(AutofillProfile::Source::kAccount, false),
         std::tuple(AutofillProfile::Source::kLocalOrSyncable, true),
         std::tuple(AutofillProfile::Source::kAccount, true)}),
    [](const ::testing::TestParamInfo<
        SaveUpdateAddressProfilePromptControllerTest::ParamType>& info) {
      auto suffix = std::string();

      return std::string() +
             (std::get<0>(info.param) == AutofillProfile::Source::kAccount
                  ? "ProfileFromAccount"
                  : "LocalOrSyncProfile") +
             (std::get<1>(info.param) ? "WithProfileMigration"
                                      : "WithoutProfileMigration");
    });

}  // namespace autofill
