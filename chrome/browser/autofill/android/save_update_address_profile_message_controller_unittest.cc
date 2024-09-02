// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_message_controller.h"

#include "base/android/jni_android.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using testing::_;
using profile_ref = base::optional_ref<const AutofillProfile>;
using ::testing::Property;

class SaveUpdateAddressProfileMessageControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveUpdateAddressProfileMessageControllerTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;
  TestingProfile::TestingFactories GetTestingFactories() const override;

  void SigninUser(const std::string& email, signin::ConsentLevel consent_level);
  void EnqueueSaveMessage(
      const AutofillProfile& profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback
          action_callback) {
    EnqueueMessage(profile, nullptr, is_migration_to_account,
                   std::move(save_callback), std::move(action_callback));
  }
  void EnqueueUpdateMessage(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback
          action_callback) {
    EnqueueMessage(profile, original_profile, /*is_migration_to_account=*/false,
                   std::move(save_callback), std::move(action_callback));
  }
  void ExpectDismissMessageCall();

  void TriggerActionClick();
  void TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* GetMessageWrapper();

  std::unique_ptr<AutofillProfile> profile_;
  std::unique_ptr<AutofillProfile> original_profile_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      save_callback_;
  base::MockCallback<
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback>
      action_callback_;

 private:
  void EnqueueMessage(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback save_callback,
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback
          action_callback);

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  SaveUpdateAddressProfileMessageController controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  base::test::ScopedFeatureList feature_list_;
};

void SaveUpdateAddressProfileMessageControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

  profile_ = std::make_unique<AutofillProfile>(test::GetFullProfile());
  original_profile_ =
      std::make_unique<AutofillProfile>(test::GetFullProfile2());
}

void SaveUpdateAddressProfileMessageControllerTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

TestingProfile::TestingFactories
SaveUpdateAddressProfileMessageControllerTest::GetTestingFactories() const {
  return IdentityTestEnvironmentProfileAdaptor::
      GetIdentityTestEnvironmentFactories();
}

void SaveUpdateAddressProfileMessageControllerTest::SigninUser(
    const std::string& email,
    signin::ConsentLevel consent_level) {
  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      email, consent_level);
}

void SaveUpdateAddressProfileMessageControllerTest::EnqueueMessage(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AutofillClient::AddressProfileSavePromptCallback save_callback,
    SaveUpdateAddressProfileMessageController::PrimaryActionCallback
        action_callback) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  controller_.DisplayMessage(web_contents(), profile, original_profile,
                             is_migration_to_account, std::move(save_callback),
                             std::move(action_callback));
  EXPECT_TRUE(controller_.IsMessageDisplayed());
}

void SaveUpdateAddressProfileMessageControllerTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SaveUpdateAddressProfileMessageControllerTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  EXPECT_TRUE(controller_.IsMessageDisplayed());
}

void SaveUpdateAddressProfileMessageControllerTest::
    TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
  EXPECT_FALSE(controller_.IsMessageDisplayed());
}

messages::MessageWrapper*
SaveUpdateAddressProfileMessageControllerTest::GetMessageWrapper() {
  return controller_.message_.get();
}

// Tests that the save message properties (title, description with profile
// details, primary button text, icon) are set correctly during local or sync
// address profile saving process.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       SaveMessageContent_LocalOrSyncAddressProfile) {
  EnqueueSaveMessage(*profile_, /*is_migration_to_account=*/false,
                     save_callback_.Get(), action_callback_.Get());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(u"John H. Doe, 666 Erebus St.",
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileMessageController::kDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the save message properties (title, description with profile
// details, primary button text, icon) are set correctly during address profile
// migration flow.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       SaveMessageContent_AddressProfileMigrationFlow) {
  test_api(*profile_).set_record_type(AutofillProfile::RecordType::kAccount);
  test_api(*original_profile_)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  EnqueueSaveMessage(*profile_, /*is_migration_to_account=*/true,
                     save_callback_.Get(), action_callback_.Get());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IN_ACCOUNT_MESSAGE_ADDRESS_MIGRATION_RECORD_TYPE_NOTICE),
      GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileMessageController::kDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_UPLOAD_ADDRESS),
      GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the save message properties (title, description with profile
// details, primary button text, icon) are set correctly when a new address
// profile is saved in account.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       SaveMessageContent_AccountAddressProfile) {
  test_api(*profile_).set_record_type(AutofillProfile::RecordType::kAccount);
  test_api(*original_profile_)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  EnqueueSaveMessage(*profile_, /*is_migration_to_account=*/false,
                     save_callback_.Get(), action_callback_.Get());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_SAVE_IN_ACCOUNT_MESSAGE_ADDRESS_RECORD_TYPE_NOTICE,
                base::ASCIIToUTF16(TestingProfile::kDefaultProfileUserName)),
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileMessageController::kDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the update message properties (title, description with original
// profile details, primary button text, icon) are set correctly.
TEST_F(SaveUpdateAddressProfileMessageControllerTest, UpdateMessageContent) {
  EnqueueUpdateMessage(*profile_, original_profile_.get(), save_callback_.Get(),
                       action_callback_.Get());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(u"Jane A. Smith, 123 Main Street",
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(SaveUpdateAddressProfileMessageController::kDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button of the save message.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       ProceedOnActionClickWhenSave) {
  EnqueueSaveMessage(*profile_, /*is_migration_to_account=*/false,
                     save_callback_.Get(), action_callback_.Get());

  EXPECT_CALL(action_callback_, Run(_, *profile_, nullptr, false, _));
  TriggerActionClick();

  EXPECT_CALL(save_callback_, Run(_, Property(&profile_ref::has_value, false)))
      .Times(0);
  TriggerMessageDismissedCallback(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button of the update message.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       ProceedOnActionClickWhenUpdate) {
  EnqueueUpdateMessage(*profile_, original_profile_.get(), save_callback_.Get(),
                       action_callback_.Get());

  EXPECT_CALL(action_callback_,
              Run(_, *profile_, original_profile_.get(), _, _));
  TriggerActionClick();

  EXPECT_CALL(save_callback_, Run(_, Property(&profile_ref::has_value, false)))
      .Times(0);
  TriggerMessageDismissedCallback(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that the save callback is triggered with
// |AddressPromptUserDecision::kMessageDeclined| when the user
// dismisses the message via gesture.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       DecisionIsMessageDeclinedOnGestureDismiss) {
  EnqueueSaveMessage(*profile_, /*is_migration_to_account=*/false,
                     save_callback_.Get(), action_callback_.Get());

  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kMessageDeclined,
                  Property(&profile_ref::has_value, false)));
  TriggerMessageDismissedCallback(messages::DismissReason::GESTURE);
}

// Tests that the save callback is triggered with
// |AddressPromptUserDecision::kMessageTimeout| when the message is
// auto-dismissed after a timeout.
TEST_F(SaveUpdateAddressProfileMessageControllerTest,
       DecisionIsMessageTimeoutOnTimerAutodismiss) {
  EnqueueSaveMessage(*profile_, /*is_migration_to_account=*/false,
                     save_callback_.Get(), action_callback_.Get());

  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kMessageTimeout,
                  Property(&profile_ref::has_value, false)));
  TriggerMessageDismissedCallback(messages::DismissReason::TIMER);
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveUpdateAddressProfileMessageControllerTest, OnlyOnePromptAtATime) {
  EnqueueUpdateMessage(*profile_, original_profile_.get(), save_callback_.Get(),
                       action_callback_.Get());

  AutofillProfile another_profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  base::MockCallback<
      SaveUpdateAddressProfileMessageController::PrimaryActionCallback>
      another_action_callback;
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                  Property(&profile_ref::has_value, false)));
  ExpectDismissMessageCall();
  EnqueueSaveMessage(another_profile, /*is_migration_to_account=*/false,
                     another_save_callback.Get(),
                     another_action_callback.Get());

  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

}  // namespace autofill
