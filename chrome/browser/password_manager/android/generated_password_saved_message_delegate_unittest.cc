// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/android/generated_password_saved_message_delegate.h"
#include <algorithm>
#include <memory>
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/android/add_username_dialog/add_username_dialog_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordFormManagerForUI;

namespace {
constexpr char16_t kDefaultUrl[] = u"http://example.com";
constexpr char16_t kUsername[] = u"username_42";
constexpr char16_t kPassword[] = u"password_42";

using testing::_;
using testing::AtMost;
using testing::InvokeWithoutArgs;
using testing::Sequence;

class MockJniDelegate : public AddUsernameDialogBridge::JniDelegate {
 public:
  MockJniDelegate() = default;
  ~MockJniDelegate() override = default;

  MOCK_METHOD((void),
              Create,
              (const gfx::NativeWindow, AddUsernameDialogBridge*),
              (override));
  MOCK_METHOD((void),
              ShowAddUsernameDialog,
              (const std::u16string&),
              (override));
  MOCK_METHOD((void), Dismiss, (), (override));
};

}  // namespace

class GeneratedPasswordSavedMessageDelegateTest
    : public base::test::WithFeatureOverride,
      public ChromeRenderViewHostTestHarness {
 public:
  GeneratedPasswordSavedMessageDelegateTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManager(
      const GURL& url);

  void SetUsernameAndPassword(std::u16string username, std::u16string password);
  void DismissMessage();
  messages::MessageWrapper* GetMessageWrapper() {
    return delegate_->message_.get();
  }

  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<GeneratedPasswordSavedMessageDelegate> delegate_;
  raw_ptr<MockJniDelegate> username_bridge_jni_raw_ptr_;

 private:
  password_manager::PasswordForm form_;
  GURL password_form_url_;
  base::MockCallback<
      GeneratedPasswordSavedMessageDelegate::CreateAddUsernameDialogBridge>
      mock_create_add_username_bridge_factory_;
  std::unique_ptr<MockJniDelegate> username_bridge_jni_;
};

GeneratedPasswordSavedMessageDelegateTest::
    GeneratedPasswordSavedMessageDelegateTest()
    : base::test::WithFeatureOverride(
          password_manager::features::kPasswordGenerationBottomSheet),
      username_bridge_jni_(std::make_unique<MockJniDelegate>()) {
  EXPECT_CALL(mock_create_add_username_bridge_factory_, Run)
      .Times(AtMost(1))
      .WillOnce([this]() {
        return std::make_unique<AddUsernameDialogBridge>(
            base::PassKey<class GeneratedPasswordSavedMessageDelegateTest>(),
            std::move(username_bridge_jni_));
      });
  // username_bridge_jni_ will be moved into the delegate class after calling
  // `ShowPrompt`, so we are saving the pointer here to verify the calls on it
  // later.
  username_bridge_jni_raw_ptr_ = username_bridge_jni_.get();
  delegate_ = std::make_unique<GeneratedPasswordSavedMessageDelegate>(
      base::PassKey<class GeneratedPasswordSavedMessageDelegateTest>(),
      mock_create_add_username_bridge_factory_.Get());
}

void GeneratedPasswordSavedMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  NavigateAndCommit(GURL(kDefaultUrl));
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void GeneratedPasswordSavedMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordFormManagerForUI>
GeneratedPasswordSavedMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url) {
  password_form_url_ = password_form_url;
  auto form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(testing::ReturnRef(form_));
  ON_CALL(*form_manager, GetURL())
      .WillByDefault(testing::ReturnRef(password_form_url_));
  return form_manager;
}

void GeneratedPasswordSavedMessageDelegateTest::SetUsernameAndPassword(
    std::u16string username,
    std::u16string password) {
  form_.username_value = std::move(username);
  form_.password_value = std::move(password);
}

void GeneratedPasswordSavedMessageDelegateTest::DismissMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  delegate_->DismissPromptInternal();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that message properties (title, description, icon, button text) are
// set correctly.
TEST_P(GeneratedPasswordSavedMessageDelegateTest, MessagePropertyValues) {
  SetUsernameAndPassword(kUsername, kPassword);
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));

  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_->ShowPrompt(web_contents(), std::move(form_manager));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OK),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_GENERATED_PASSWORD_SAVED_MESSAGE_DESCRIPTION),
      GetMessageWrapper()->GetDescription());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(
                IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP),
            GetMessageWrapper()->GetIconResourceId());
  DismissMessage();
}

TEST_P(GeneratedPasswordSavedMessageDelegateTest,
       AddUsernameDialogIsDisplayedWhenEmptyUsername) {
  SetUsernameAndPassword(u"", kPassword);
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));

  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationBottomSheet)) {
    EXPECT_CALL(*username_bridge_jni_raw_ptr_, ShowAddUsernameDialog);
  } else {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  }

  delegate_->ShowPrompt(web_contents(), std::move(form_manager));
}

TEST_P(GeneratedPasswordSavedMessageDelegateTest,
       DialogIsDismissedOnDelegateDestruction) {
  SetUsernameAndPassword(u"", kPassword);
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  delegate_->ShowPrompt(web_contents(), std::move(form_manager));

  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationBottomSheet)) {
    EXPECT_CALL(*username_bridge_jni_raw_ptr_, Dismiss);
  } else {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage);
  }
  delegate_.reset();
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    GeneratedPasswordSavedMessageDelegateTest);
