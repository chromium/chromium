// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/near_oom_reduction_message_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char16_t kDefaultUrl[] = u"http://example.com";
}  // namespace

class NearOomReductionMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  NearOomReductionMessageDelegateTest() {}

 protected:
  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage();
  void DismissMessage();
  void TriggerPrimaryButtonClick();

  messages::MessageWrapper* GetMessageWrapper() {
    return delegate_.message_for_testing();
  }

 private:
  oom_intervention::NearOomReductionMessageDelegate delegate_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

void NearOomReductionMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  NavigateAndCommit(GURL(kDefaultUrl));
}

void NearOomReductionMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void NearOomReductionMessageDelegateTest::EnqueueMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::Return(true));
  delegate_.ShowMessage(web_contents(), nullptr);
}

void NearOomReductionMessageDelegateTest::DismissMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  delegate_.DismissMessage(messages::DismissReason::UNKNOWN);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void NearOomReductionMessageDelegateTest::TriggerPrimaryButtonClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

// Tests that message properties (title, description, icon, button text)
// are set correctly.
TEST_F(NearOomReductionMessageDelegateTest, MessagePropertyValues) {
  EnqueueMessage();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEAR_OOM_REDUCTION_MESSAGE_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEAR_OOM_REDUCTION_MESSAGE_DESCRIPTION),
      GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SHOW_CONTENT),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_MOBILE_FRIENDLY),
            GetMessageWrapper()->GetIconResourceId());

  DismissMessage();
}  // namespace oom_intervention
