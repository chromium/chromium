// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tip_message_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/lookalikes/safety_tip_ui_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char16_t kDefaultUrl[] = u"http://example.com";
constexpr char16_t kSuggestUrl[] = u"http://google.com";
}  // namespace

class TestNavigationDelegate : public content::WebContentsDelegate {
 public:
  ~TestNavigationDelegate() override {}
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    opened_++;
    return source;
  }

  int opened() const { return opened_; }

 private:
  int opened_ = 0;
};

class SafetyTipMessageDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SafetyTipMessageDelegateAndroidTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage(
      base::OnceCallback<void(SafetyTipInteraction)> close_callback,
      bool enqueue_expected,
      security_state::SafetyTipStatus safety_tip_status);
  void DismissMessage();
  void TriggerPrimaryButtonClick();
  void TriggerSecondaryButtonClick();

  messages::MessageWrapper* GetMessageWrapper() {
    return delegate_.message_.get();
  }

  TestNavigationDelegate* GetTestNavigationDelegate() {
    return &mock_web_contents_delegate_;
  }

 private:
  SafetyTipMessageDelegateAndroid delegate_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  TestNavigationDelegate mock_web_contents_delegate_;
};

void SafetyTipMessageDelegateAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  NavigateAndCommit(GURL(kDefaultUrl));
}

void SafetyTipMessageDelegateAndroidTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void SafetyTipMessageDelegateAndroidTest::EnqueueMessage(
    base::OnceCallback<void(SafetyTipInteraction)> close_callback,
    bool enqueue_expected,
    security_state::SafetyTipStatus safety_tip_status) {
  if (enqueue_expected) {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
        .WillOnce(testing::Return(true));
  } else {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
  }
  delegate_.DisplaySafetyTipPrompt(safety_tip_status, GURL(kSuggestUrl),
                                   web_contents(), std::move(close_callback));
}

void SafetyTipMessageDelegateAndroidTest::DismissMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  delegate_.DismissInternal();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void SafetyTipMessageDelegateAndroidTest::TriggerPrimaryButtonClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SafetyTipMessageDelegateAndroidTest::TriggerSecondaryButtonClick() {
  GetMessageWrapper()->HandleSecondaryActionClick(
      base::android::AttachCurrentThread());
}

TEST_F(SafetyTipMessageDelegateAndroidTest, DismissOnNoAction) {
  base::MockOnceCallback<void(SafetyTipInteraction)> mock_callback_receiver;
  EnqueueMessage(mock_callback_receiver.Get(), true,
                 security_state::SafetyTipStatus::kLookalike);
  EXPECT_CALL(mock_callback_receiver, Run(SafetyTipInteraction::kNoAction));
  DismissMessage();
}

TEST_F(SafetyTipMessageDelegateAndroidTest, DoNotReplaceCurrentMessage) {
  base::MockOnceCallback<void(SafetyTipInteraction)> mock_callback_receiver;
  EnqueueMessage(mock_callback_receiver.Get(), true,
                 security_state::SafetyTipStatus::kLookalike);
  EXPECT_CALL(mock_callback_receiver, Run(SafetyTipInteraction::kNoAction))
      .Times(0);
  EnqueueMessage(mock_callback_receiver.Get(), false,
                 security_state::SafetyTipStatus::kLookalike);
  EXPECT_CALL(mock_callback_receiver, Run(SafetyTipInteraction::kNoAction))
      .Times(1);
  DismissMessage();
}

TEST_F(SafetyTipMessageDelegateAndroidTest, PrimaryActionCallback) {
  base::MockOnceCallback<void(SafetyTipInteraction)> mock_callback_receiver;
  EnqueueMessage(mock_callback_receiver.Get(), true,
                 security_state::SafetyTipStatus::kLookalike);

  web_contents()->SetDelegate(GetTestNavigationDelegate());
  TriggerPrimaryButtonClick();
  EXPECT_EQ(GetTestNavigationDelegate()->opened(), 1);

  DismissMessage();
}

TEST_F(SafetyTipMessageDelegateAndroidTest, SecondaryActionCallback) {
  base::MockOnceCallback<void(SafetyTipInteraction)> mock_callback_receiver;
  EnqueueMessage(mock_callback_receiver.Get(), true,
                 security_state::SafetyTipStatus::kLookalike);

  web_contents()->SetDelegate(GetTestNavigationDelegate());
  TriggerSecondaryButtonClick();
  EXPECT_EQ(GetTestNavigationDelegate()->opened(), 1);

  DismissMessage();
}

TEST_F(SafetyTipMessageDelegateAndroidTest, MessagePropertyValuesLookAlike) {
  base::MockOnceCallback<void(SafetyTipInteraction)> mock_callback_receiver;
  security_state::SafetyTipStatus status =
      security_state::SafetyTipStatus::kLookalike;
  EnqueueMessage(mock_callback_receiver.Get(), true, status);

  EXPECT_EQ(GetSafetyTipTitle(status, GURL(kSuggestUrl)),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(GetSafetyTipDescription(status, GURL(kSuggestUrl)),
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(GetSafetyTipLeaveButtonId(status)),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAFETYTIP_SHIELD),
      GetMessageWrapper()->GetIconResourceId());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_MORE_INFO_LINK),
            GetMessageWrapper()->GetSecondaryButtonMenuText());
  DismissMessage();
}
