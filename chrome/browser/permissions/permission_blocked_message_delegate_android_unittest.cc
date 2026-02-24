// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blocked_message_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/common/features.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;

}  // namespace

class MockPermissionPromptAndroid
    : public permissions::PermissionPromptAndroid {
 public:
  MockPermissionPromptAndroid(content::WebContents* web_contents,
                              permissions::PermissionPrompt::Delegate* delegate)
      : permissions::PermissionPromptAndroid(web_contents, delegate) {}
  ~MockPermissionPromptAndroid() override = default;

  MOCK_METHOD(const std::vector<base::WeakPtr<permissions::PermissionRequest>>&,
              Requests,
              (),
              (const, override));

  MOCK_METHOD(permissions::PermissionPromptDisposition,
              GetPromptDisposition,
              (),
              (const, override));

  base::WeakPtr<MockPermissionPromptAndroid> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPermissionPromptAndroid> weak_factory_{this};
};

class MockDelegate : public PermissionBlockedMessageDelegate::Delegate {
 public:
  ~MockDelegate() override = default;
  MockDelegate(const base::WeakPtr<permissions::PermissionPromptAndroid>&
                   permission_prompt)
      : PermissionBlockedMessageDelegate::Delegate(permission_prompt) {}

  MOCK_METHOD(void, Accept, (), (override));
  MOCK_METHOD(void, Deny, (), (override));

  MOCK_METHOD(void, Dismiss, (), (override));
  MOCK_METHOD(void, Ignore, (), (override));

  MOCK_METHOD(bool, ShouldUseQuietUI, (), (override));
  MOCK_METHOD(std::optional<QuietUiReason>,
              ReasonForUsingQuietUi,
              (),
              (override));
  MOCK_METHOD(ContentSettingsType, GetContentSettingsType, (), (override));
  MOCK_METHOD(void, SwitchToLoudPrompt, (), (override));
};

class TestPermissionBlockedMessageDelegate
    : public PermissionBlockedMessageDelegate {
 public:
  TestPermissionBlockedMessageDelegate(content::WebContents* web_contents,
                                       std::unique_ptr<Delegate> delegate)
      : PermissionBlockedMessageDelegate(web_contents, std::move(delegate)) {}
  ~TestPermissionBlockedMessageDelegate() override = default;

  void ResolveWithOSPrompt(ContentSettingsType content_settings_type) override {
    // Simulate Java callback calling Accept on native.
    delegate_->Accept();
  }
};

class PermissionBlockedMessageDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PermissionBlockedMessageDelegateAndroidTest() = default;
  ~PermissionBlockedMessageDelegateAndroidTest() override = default;

  void ExpectEnqueued() {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  }

  void ExpectDismiss() {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([](messages::MessageWrapper* message,
                     messages::DismissReason dismiss_reason) {
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(dismiss_reason));
        });
  }

  void ShowMessage(std::unique_ptr<MockDelegate> delegate) {
    controller_ = std::make_unique<TestPermissionBlockedMessageDelegate>(
        web_contents(), std::move(delegate));
  }

  void TriggerDismiss(messages::DismissReason reason) {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([&reason](messages::MessageWrapper* message,
                            messages::DismissReason dismiss_reason) {
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(reason));
        });
    controller_->DismissInternal();
    EXPECT_EQ(nullptr, controller_->message_.get());
  }

  void TriggerPrimaryAction() {
    controller_->HandleQuietPrimaryActionClick();
    TriggerDismiss(messages::DismissReason::PRIMARY_ACTION);
  }

  void TriggerLoudPrimaryAction() {
    controller_->HandleLoudPrimaryActionClick();
    TriggerDismiss(messages::DismissReason::PRIMARY_ACTION);
  }

  void TriggerLoudSecondaryMenuItem(int command_id) {
    controller_->HandleLoudUiSecondayMenuItemClicked(command_id);
  }

  std::unique_ptr<MockDelegate> CreateDelegateWithPrompt(
      std::unique_ptr<MockPermissionPromptAndroid>& prompt_storage,
      const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
          requests) {
    prompt_storage =
        std::make_unique<MockPermissionPromptAndroid>(web_contents(), manager_);
    EXPECT_CALL(*prompt_storage, Requests)
        .WillRepeatedly(testing::ReturnRef(requests));

    return std::make_unique<MockDelegate>(prompt_storage->GetWeakPtr());
  }

  void TriggerManageClick() { controller_->HandleManageClick(); }

  void TriggerDialogOnAllowForThisSite() {
    controller_->OnAllowForThisSite();
    controller_->OnDialogDismissed();
  }

  void TriggerDialogDismiss() { controller_->OnDialogDismissed(); }

  messages::MessageWrapper* GetMessageWrapper() {
    return controller_->message_.get();
  }

  std::unique_ptr<MockDelegate> GetMockDelegate() {
    return std::move(delegate_);
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<PermissionBlockedMessageDelegate> controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockDelegate> delegate_;
  raw_ptr<permissions::PermissionRequestManager> manager_ = nullptr;
};

void PermissionBlockedMessageDelegateAndroidTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  delegate_ = std::make_unique<MockDelegate>(nullptr);
  permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  manager_ =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
}

void PermissionBlockedMessageDelegateAndroidTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, DismissByTimeout) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);
  EXPECT_CALL(*delegate, Ignore).Times(1);
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  ShowMessage(std::move(delegate));
  TriggerDismiss(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, DismissByPrimaryAction) {
  auto delegate = GetMockDelegate();
  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny);
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  ShowMessage(std::move(delegate));
  TriggerPrimaryAction();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, DismissByDialogDismissed) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ReasonForUsingQuietUi)
      .WillRepeatedly(testing::Return(
          std::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);
  EXPECT_CALL(*delegate, Dismiss);

  ShowMessage(std::move(delegate));

  TriggerManageClick();
  TriggerDismiss(messages::DismissReason::SECONDARY_ACTION);
  TriggerDialogDismiss();
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest,
       DismissByDialogOnAllowForThisSite) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ReasonForUsingQuietUi)
      .WillRepeatedly(testing::Return(
          std::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  EXPECT_CALL(*delegate, Accept);
  EXPECT_CALL(*delegate, Deny).Times(0);

  ShowMessage(std::move(delegate));

  TriggerManageClick();
  TriggerDismiss(messages::DismissReason::SECONDARY_ACTION);
  TriggerDialogOnAllowForThisSite();
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, LoudUI_Shown) {
  // Setup request
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests;
  requests.push_back(request->GetWeakPtr());

  std::unique_ptr<MockPermissionPromptAndroid> mock_prompt;
  auto delegate = CreateDelegateWithPrompt(mock_prompt, requests);

  // Expect ShouldUseQuietUI to return false for Loud UI
  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  ShowMessage(std::move(delegate));

  messages::MessageWrapper* message = GetMessageWrapper();
  ASSERT_TRUE(message);

  // Verify Title
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_TITLE_MESSAGE_UI),
            message->GetTitle());

  // Verify Description contains origin
  std::u16string origin = url_formatter::FormatUrlForSecurityDisplay(
      request->requesting_origin(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  EXPECT_NE(std::u16string::npos, message->GetDescription().find(origin));

  // Verify Primary Button
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW),
            message->GetPrimaryButtonText());
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest,
       LoudUI_DismissByPrimaryAction) {
  base::HistogramTester histogram_tester;

  // Setup request
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests;
  requests.push_back(request->GetWeakPtr());

  std::unique_ptr<MockPermissionPromptAndroid> mock_prompt;
  auto delegate = CreateDelegateWithPrompt(mock_prompt, requests);

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  // Expect Accept() to be called on primary action (Allow)
  EXPECT_CALL(*delegate, Accept()).Times(1);

  ExpectEnqueued();

  // Show Message
  ShowMessage(std::move(delegate));

  // Trigger Primary Action (Allow)
  TriggerLoudPrimaryAction();

  // Message should be dismissed
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Verify Histogram
  histogram_tester.ExpectBucketCount("Permissions.ClapperLoud.MessageUI.Allow",
                                     true, 1);
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, LoudUI_DismissByGesture) {
  base::HistogramTester histogram_tester;

  // Setup request
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests;
  requests.push_back(request->GetWeakPtr());

  std::unique_ptr<MockPermissionPromptAndroid> mock_prompt;
  auto delegate = CreateDelegateWithPrompt(mock_prompt, requests);

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  // Expect Deny() to be called on gesture dismiss
  EXPECT_CALL(*delegate, Deny()).Times(1);
  EXPECT_CALL(*delegate, Accept()).Times(0);

  ExpectEnqueued();

  ShowMessage(std::move(delegate));

  // Trigger Dismiss by Gesture
  TriggerDismiss(messages::DismissReason::GESTURE);

  // Message should be dismissed
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Verify Histogram
  histogram_tester.ExpectBucketCount(
      "Permissions.ClapperLoud.MessageUI.Dismiss", true, 1);
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, LoudUI_DismissByTimeout) {
  // Setup request
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests;
  requests.push_back(request->GetWeakPtr());

  std::unique_ptr<MockPermissionPromptAndroid> mock_prompt;
  auto delegate = CreateDelegateWithPrompt(mock_prompt, requests);

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  // Expect Ignore() to be called on timer dismiss
  EXPECT_CALL(*delegate, Ignore()).Times(1);
  EXPECT_CALL(*delegate, Accept()).Times(0);
  EXPECT_CALL(*delegate, Deny()).Times(0);

  ExpectEnqueued();

  ShowMessage(std::move(delegate));

  // Trigger Dismiss by Timer
  TriggerDismiss(messages::DismissReason::TIMER);

  // Message should be dismissed
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest,
       SecondaryActionWithApproximateLocation) {
  base::test::ScopedFeatureList scoped_feature_list{
      content_settings::features::kApproximateGeolocationPermission};
  std::unique_ptr<MockDelegate> delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ReasonForUsingQuietUi)
      .WillRepeatedly(testing::Return(
          std::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(
          testing::Return(ContentSettingsType::GEOLOCATION_WITH_OPTIONS));

  ExpectEnqueued();

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);
  EXPECT_CALL(*delegate, SwitchToLoudPrompt);

  ShowMessage(std::move(delegate));

  TriggerManageClick();
  TriggerDismiss(messages::DismissReason::SECONDARY_ACTION);
  TriggerDialogDismiss();
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest, LoudUI_SecondaryMenu_Deny) {
  base::HistogramTester histogram_tester;

  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests;
  requests.push_back(request->GetWeakPtr());

  std::unique_ptr<MockPermissionPromptAndroid> mock_prompt;
  auto delegate = CreateDelegateWithPrompt(mock_prompt, requests);

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  // Expect Deny() to be called on menu "Don't Allow"
  EXPECT_CALL(*delegate, Deny()).Times(1);

  ExpectEnqueued();

  ShowMessage(std::move(delegate));

  // Trigger Menu "Don't Allow" (kDeny = 0)
  TriggerLoudSecondaryMenuItem(0);

  // Verify Histogram
  histogram_tester.ExpectBucketCount("Permissions.ClapperLoud.MessageUI.Deny",
                                     true, 1);
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest,
       LoudUI_SecondaryMenu_Manage) {
  base::HistogramTester histogram_tester;

  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests;
  requests.push_back(request->GetWeakPtr());

  std::unique_ptr<MockPermissionPromptAndroid> mock_prompt;
  auto delegate = CreateDelegateWithPrompt(mock_prompt, requests);

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::NOTIFICATIONS));

  ExpectEnqueued();

  ShowMessage(std::move(delegate));

  // Expect message to be dismissed with SECONDARY_ACTION
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        EXPECT_EQ(messages::DismissReason::SECONDARY_ACTION, dismiss_reason);
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });

  // Trigger Menu "Manage" (kManage = 1)
  TriggerLoudSecondaryMenuItem(1);

  // Verify Histogram
  histogram_tester.ExpectBucketCount("Permissions.ClapperLoud.MessageUI.Manage",
                                     true, 1);
}

TEST_F(PermissionBlockedMessageDelegateAndroidTest,
       DismissByDialogDismissed_GestureGated) {
  auto delegate = GetMockDelegate();

  EXPECT_CALL(*delegate, ShouldUseQuietUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate, ReasonForUsingQuietUi)
      .WillRepeatedly(testing::Return(std::optional<QuietUiReason>(
          QuietUiReason::kTriggeredDueToLackOfGesture)));
  EXPECT_CALL(*delegate, GetContentSettingsType)
      .WillRepeatedly(testing::Return(ContentSettingsType::GEOLOCATION));

  ExpectEnqueued();

  EXPECT_CALL(*delegate, Accept).Times(0);
  EXPECT_CALL(*delegate, Deny).Times(0);
  EXPECT_CALL(*delegate, Dismiss);

  ShowMessage(std::move(delegate));

  TriggerManageClick();
  TriggerDismiss(messages::DismissReason::SECONDARY_ACTION);
  TriggerDialogDismiss();
}
