// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/guid.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class MockSaveAddressProfilePromptView : public SaveAddressProfilePromptView {
 public:
  MOCK_METHOD(bool,
              Show,
              (SaveAddressProfilePromptController * controller,
               const AutofillProfile& autofill_profile,
               bool is_update),
              (override));
  MOCK_METHOD(void, RefreshContent, (), (override));
};

class SaveAddressProfilePromptControllerTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable both explicit save prompts and structured names.
    feature_list_.InitWithFeatures(
        {features::kAutofillAddressProfileSavePrompt,
         features::kAutofillEnableSupportForMoreStructureInNames},
        {});

    profile_ = test::GetFullProfile();
    original_profile_ = test::GetFullProfile();
    original_profile_.SetInfo(NAME_FULL, u"John Doe", GetLocale());
    original_profile_.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"", GetLocale());
    SetUpController(/*is_update=*/false);

    CountryNames::SetLocaleString(GetLocale());
  }

  // Profile with raw data as it is returned from Java.
  AutofillProfile GetFullProfileNoStatus() {
    AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
    profile.SetRawInfo(NAME_FULL, u"Mona J. Liza");
    test::SetProfileInfo(&profile, "", "", "", "email@example.com",
                         "Company Inc.", "33 Narrow Street", "Apt 42",
                         "Playa Vista", "LA", "12345", "US", "13105551234",
                         /*finalize=*/true,
                         structured_address::VerificationStatus::kNoStatus);
    return profile;
  }

 protected:
  void SetUpController(bool is_update);

  std::string GetLocale() { return "en-US"; }

  base::test::ScopedFeatureList feature_list_;
  MockSaveAddressProfilePromptView* prompt_view_;
  AutofillProfile profile_;
  AutofillProfile original_profile_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      decision_callback_;
  base::MockCallback<base::OnceCallback<void()>> dismissal_callback_;
  std::unique_ptr<SaveAddressProfilePromptController> controller_;
  JNIEnv* env_ = base::android::AttachCurrentThread();
  base::android::JavaParamRef<jobject> mock_caller_{nullptr};
};

void SaveAddressProfilePromptControllerTest::SetUpController(bool is_update) {
  auto prompt_view = std::make_unique<MockSaveAddressProfilePromptView>();
  prompt_view_ = prompt_view.get();
  controller_ = std::make_unique<SaveAddressProfilePromptController>(
      std::move(prompt_view), profile_,
      is_update ? &original_profile_ : nullptr, decision_callback_.Get(),
      dismissal_callback_.Get());
  ON_CALL(*prompt_view_, Show(controller_.get(), profile_, is_update))
      .WillByDefault(testing::Return(true));
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPrompt_Save) {
  EXPECT_CALL(*prompt_view_, Show(controller_.get(), profile_, false));
  controller_->DisplayPrompt();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldShowViewOnDisplayPrompt_Update) {
  SetUpController(/*is_update=*/true);
  EXPECT_CALL(*prompt_view_, Show(controller_.get(), profile_, true));
  controller_->DisplayPrompt();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenShowReturnsFalse) {
  EXPECT_CALL(*prompt_view_, Show(controller_.get(), profile_, false))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->DisplayPrompt();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserAccepts) {
  controller_->DisplayPrompt();

  EXPECT_CALL(
      decision_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile_));
  controller_->OnUserAccepted(env_, mock_caller_);
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserDeclines) {
  controller_->DisplayPrompt();

  EXPECT_CALL(
      decision_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          profile_));
  controller_->OnUserDeclined(env_, mock_caller_);
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserAcceptsAfterEditingTheProfile) {
  controller_->DisplayPrompt();

  AutofillProfile edited_profile = GetFullProfileNoStatus();
  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEdited,
                  edited_profile));
  base::android::ScopedJavaLocalRef<jobject> edited_profile_java =
      PersonalDataManagerAndroid::CreateJavaProfileFromNative(env_,
                                                              edited_profile);
  controller_->OnUserEdited(
      env_, mock_caller_,
      base::android::JavaParamRef<jobject>(env_, edited_profile_java.obj()));
  controller_->OnUserAccepted(env_, mock_caller_);
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenPromptIsDismissed) {
  controller_->DisplayPrompt();

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->OnPromptDismissed(env_, mock_caller_);
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenControllerDiesWithoutInteraction) {
  controller_->DisplayPrompt();

  EXPECT_CALL(decision_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));
  controller_.reset();
}

TEST_F(SaveAddressProfilePromptControllerTest, ShouldReturnDataToDisplay_Save) {
  EXPECT_EQ(u"Save address?", controller_->GetTitle());
  EXPECT_EQ(
      u"John H. Doe\nUnderworld\n666 Erebus St.\nApt 8\nElysium, CA "
      u"91111\nUnited States",
      controller_->GetAddress());
  EXPECT_EQ(u"johndoe@hades.com", controller_->GetEmail());
  EXPECT_EQ(u"16502111111", controller_->GetPhoneNumber());
  EXPECT_EQ(u"Save", controller_->GetPositiveButtonText());
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldReturnDataToDisplay_Update) {
  SetUpController(/*is_update=*/true);
  EXPECT_EQ(u"Update address?", controller_->GetTitle());
  EXPECT_EQ(u"For John Doe — 666 Erebus St.", controller_->GetSubtitle());
  std::pair<std::u16string, std::u16string> differences =
      controller_->GetDiffFromOldToNewProfile();
  EXPECT_EQ(u"John Doe", differences.first);
  EXPECT_EQ(u"John H. Doe\n16502111111", differences.second);
  EXPECT_EQ(u"Update", controller_->GetPositiveButtonText());
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldRefreshContentAfterEditingTheProfile) {
  controller_->DisplayPrompt();

  AutofillProfile edited_profile = GetFullProfileNoStatus();
  base::android::ScopedJavaLocalRef<jobject> edited_profile_java =
      PersonalDataManagerAndroid::CreateJavaProfileFromNative(env_,
                                                              edited_profile);
  EXPECT_CALL(*prompt_view_, RefreshContent());
  controller_->OnUserEdited(
      env_, mock_caller_,
      base::android::JavaParamRef<jobject>(env_, edited_profile_java.obj()));

  // Also the getters should return data for the `edited_profile`.
  EXPECT_EQ(
      u"Mona J. Liza\nCompany Inc.\n33 Narrow Street\nApt 42\nPlaya Vista, LA "
      u"12345\nUnited States",
      controller_->GetAddress());
  EXPECT_EQ(u"email@example.com", controller_->GetEmail());
  EXPECT_EQ(u"13105551234", controller_->GetPhoneNumber());
}

}  // namespace autofill
