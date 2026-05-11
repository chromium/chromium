// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/known_interception_disclosure_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class KnownInterceptionDisclosureMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  KnownInterceptionDisclosureMessageDelegateTest() = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);

    auto clock = std::make_unique<base::SimpleTestClock>();
    clock_ = clock.get();
    clock_->SetNow(base::Time::Now());
    KnownInterceptionDisclosureCooldown::GetInstance()->SetClockForTesting(
        std::move(clock));

    KnownInterceptionDisclosureMessageDelegate::CreateForWebContents(
        web_contents());

    EXPECT_CALL(*message_dispatcher_bridge(),
                DismissMessage(testing::_, testing::_))
        .Times(testing::AnyNumber());
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  }

  KnownInterceptionDisclosureMessageDelegate* delegate() {
    return KnownInterceptionDisclosureMessageDelegate::FromWebContents(
        web_contents());
  }

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  base::SimpleTestClock* clock() { return clock_; }

  void DismissMessage(messages::DismissReason dismiss_reason) {
    delegate()->message_->HandleDismissCallback(
        base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
  }

  void TriggerPrimaryAction() {
    delegate()->message_->HandleActionClick(
        base::android::AttachCurrentThread());
  }

 private:
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  raw_ptr<base::SimpleTestClock> clock_;
};

TEST_F(KnownInterceptionDisclosureMessageDelegateTest,
       MessageEnqueuedAndCooldownAppliedOnDismiss) {
  // Cooldown is not active initially.
  EXPECT_FALSE(
      KnownInterceptionDisclosureCooldown::GetInstance()->IsActive(profile()));

  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage);
  delegate()->MaybeShow();

  // Dismissing the message should activate the cooldown.
  DismissMessage(messages::DismissReason::UNKNOWN);
  EXPECT_TRUE(
      KnownInterceptionDisclosureCooldown::GetInstance()->IsActive(profile()));

  // Advancing the clock by 8 days should expire the cooldown (7 days).
  clock()->Advance(base::Days(8));
  EXPECT_FALSE(
      KnownInterceptionDisclosureCooldown::GetInstance()->IsActive(profile()));
}

TEST_F(KnownInterceptionDisclosureMessageDelegateTest,
       MessageEnqueuedAndCooldownAppliedOnPrimaryAction) {
  // Cooldown is not active initially.
  EXPECT_FALSE(
      KnownInterceptionDisclosureCooldown::GetInstance()->IsActive(profile()));

  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage);
  delegate()->MaybeShow();

  // Triggering the primary action should activate the cooldown.
  TriggerPrimaryAction();
  EXPECT_TRUE(
      KnownInterceptionDisclosureCooldown::GetInstance()->IsActive(profile()));
}

TEST_F(KnownInterceptionDisclosureMessageDelegateTest,
       MaybeShowDoesNothingIfAlreadyShown) {
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage).Times(1);
  delegate()->MaybeShow();
  delegate()->MaybeShow();  // Second call shouldn't enqueue again.
}
