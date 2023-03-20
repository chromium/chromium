// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/generated_password_saved_message_delegate.h"
#include "base/android/jni_android.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
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
constexpr char16_t kAccountEmail[] = u"account_42@example.com";
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
  void EnqueueMessage(std::unique_ptr<PasswordFormManagerForUI> form_to_save);
  void DismissMessage();
  messages::MessageWrapper* GetMessageWrapper() {
    return delegate_.message_.get();
  }

 private:
  GeneratedPasswordSavedMessageDelegate delegate_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  password_manager::PasswordForm form_;
  GURL password_form_url_;
};

GeneratedPasswordSavedMessageDelegateTest::
    GeneratedPasswordSavedMessageDelegateTest()
    : base::test::WithFeatureOverride(
          password_manager::features::kUnifiedPasswordManagerAndroid) {}

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

void GeneratedPasswordSavedMessageDelegateTest::EnqueueMessage(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save) {
  absl::optional<AccountInfo> account_info;
  account_info = AccountInfo();
  account_info.value().email = base::UTF16ToASCII(kAccountEmail);
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_.ShowPrompt(web_contents(), std::move(form_to_save));
}

void GeneratedPasswordSavedMessageDelegateTest::DismissMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  delegate_.DismissPromptInternal();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that message properties (title, description, icon, button text) are
// set correctly.
TEST_P(GeneratedPasswordSavedMessageDelegateTest, MessagePropertyValues) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndDisableFeature(
      password_manager::features::kUnifiedPasswordManagerAndroidBranding);

  SetUsernameAndPassword(kUsername, kPassword);
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EnqueueMessage(std::move(form_manager));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OK),
            GetMessageWrapper()->GetPrimaryButtonText());
  if (IsParamFeatureEnabled()) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_MANAGER_GENERATED_PASSWORD_SAVED_MESSAGE_DESCRIPTION),
        GetMessageWrapper()->GetDescription());
    EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(
                  IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP),
              GetMessageWrapper()->GetIconResourceId());
  } else {
    EXPECT_NE(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kUsername));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kPassword));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountEmail));
    EXPECT_EQ(
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD),
        GetMessageWrapper()->GetIconResourceId());
  }
  DismissMessage();
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    GeneratedPasswordSavedMessageDelegateTest);
