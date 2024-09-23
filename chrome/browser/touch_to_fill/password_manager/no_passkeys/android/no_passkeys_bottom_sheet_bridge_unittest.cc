// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/no_passkeys/android/no_passkeys_bottom_sheet_bridge.h"

#include <jni.h>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Sequence;
using JniDelegate = NoPasskeysBottomSheetBridge::JniDelegate;

class MockJniDelegate : public JniDelegate {
 public:
  MockJniDelegate() = default;
  ~MockJniDelegate() override = default;

  MOCK_METHOD((void), Create, (ui::WindowAndroid*), (override));
  MOCK_METHOD((void), Show, (const std::string&), (override));
  MOCK_METHOD((void), Dismiss, (), (override));
};

}  // namespace

class NoPasskeysBottomSheetBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    auto jni_delegate = std::make_unique<MockJniDelegate>();
    mock_jni_delegate_ = jni_delegate.get();
    no_passkeys_bridge_ = std::make_unique<NoPasskeysBottomSheetBridge>(
        base::PassKey<class NoPasskeysBottomSheetBridgeTest>(),
        std::move(jni_delegate));
  }

  MockJniDelegate& mock_jni_delegate() { return *mock_jni_delegate_; }

  NoPasskeysBottomSheetBridge& no_passkeys_bridge() {
    return *no_passkeys_bridge_;
  }

  void destroyNoPassleysBridge() { no_passkeys_bridge_.reset(); }

 private:
  raw_ptr<MockJniDelegate> mock_jni_delegate_;
  std::unique_ptr<NoPasskeysBottomSheetBridge> no_passkeys_bridge_;
};

TEST_F(NoPasskeysBottomSheetBridgeTest, CallDismissalDelegateOnHide) {
  auto scoped_window = ui::WindowAndroid::CreateForTesting();
  base::MockCallback<base::OnceClosure> on_dismissed_callback;
  const std::string kTestOrigin("origin.com");

  {  // Ensure the Show call creates a new bridge before showing.
    Sequence s;
    EXPECT_CALL(mock_jni_delegate(), Create);
    EXPECT_CALL(mock_jni_delegate(), Show(kTestOrigin));
  }
  no_passkeys_bridge().Show(scoped_window->get(), kTestOrigin,
                            on_dismissed_callback.Get(), base::DoNothing());

  EXPECT_CALL(on_dismissed_callback, Run);
  no_passkeys_bridge().OnDismissed(/*env=*/nullptr);
}

TEST_F(NoPasskeysBottomSheetBridgeTest, IgnoreRedundantDismissCalls) {
  auto scoped_window = ui::WindowAndroid::CreateForTesting();
  const std::string kTestOrigin("origin.com");

  {  // Ensure the Show call creates a new bridge before showing.
    Sequence s;
    EXPECT_CALL(mock_jni_delegate(), Create);
    EXPECT_CALL(mock_jni_delegate(), Show(kTestOrigin));
  }
  no_passkeys_bridge().Show(
      scoped_window->get(), kTestOrigin,
      /*on_dismissed_callback=*/base::DoNothing(),
      /*on_click_use_another_device_callback=*/base::DoNothing());

  EXPECT_CALL(mock_jni_delegate(), Dismiss)
      .WillOnce(InvokeWithoutArgs(
          [this]() { no_passkeys_bridge().OnDismissed(/*env=*/nullptr); }));
  no_passkeys_bridge().Dismiss();
  no_passkeys_bridge().Dismiss();  // This should not trigger a second call!
  destroyNoPassleysBridge();  // This also should not trigger a second call!
}

TEST_F(NoPasskeysBottomSheetBridgeTest, RunCallbackForOnClickUseAnotherDevice) {
  auto scoped_window = ui::WindowAndroid::CreateForTesting();
  base::MockCallback<base::OnceClosure> on_click_use_another_device_callback;
  const std::string kTestOrigin("origin.com");

  no_passkeys_bridge().Show(scoped_window->get(), kTestOrigin,
                            /*on_dismissed_callback=*/base::DoNothing(),
                            on_click_use_another_device_callback.Get());

  EXPECT_CALL(on_click_use_another_device_callback, Run);
  no_passkeys_bridge().OnClickUseAnotherDevice(/*env=*/nullptr);
}

TEST_F(NoPasskeysBottomSheetBridgeTest, DismissesOnDestruction) {
  auto scoped_window = ui::WindowAndroid::CreateForTesting();
  const std::string kTestOrigin("origin.com");

  no_passkeys_bridge().Show(
      scoped_window->get(), kTestOrigin,
      /*on_dismissed_callback=*/base::DoNothing(),
      /*on_click_use_another_device_callback=*/base::DoNothing());

  EXPECT_CALL(mock_jni_delegate(), Dismiss);
  destroyNoPassleysBridge();
}
