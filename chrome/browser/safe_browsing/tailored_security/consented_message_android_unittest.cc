// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace safe_browsing {
class TailoredSecurityConsentedModalAndroidTest : public testing::Test {
 protected:
  TailoredSecurityConsentedModalAndroidTest() {
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        GetProfile(), nullptr);
  }

  TestingProfile* GetProfile() { return &profile_; }

  void DoMessageAccepted(TailoredSecurityConsentedModalAndroid* modal) {
    modal->HandleMessageAccepted();
  }

  void DoMessageDismissed(TailoredSecurityConsentedModalAndroid* modal,
                          messages::DismissReason dismiss_reason) {
    modal->HandleMessageDismissed(dismiss_reason);
  }

  void DoSettingsClicked(TailoredSecurityConsentedModalAndroid* modal) {
    modal->HandleSettingsClicked();
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingBrowserProcess> browser_process_;
  // Ensure RenderFrameHostTester to be created and used by the tests.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  base::UserActionTester user_action_tester_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogHandleMessageAcceptedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing());
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "SafeBrowsing.AccountIntegration.DisabledDialog.OkButtonClicked"),
      1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogHandleMessageDismissedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing());
  DoMessageDismissed(&consented_modal, messages::DismissReason::TIMER);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.DisabledDialog.Dismissed"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogHandleSettingsClickedLogsUserAction) {
  // Create a scoped window so that WebContents::GetTopLevelNativeWindow does
  // not return null.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents_.get()->GetNativeView());
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing());
  DoSettingsClicked(&consented_modal);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.DisabledDialog."
                "SettingsButtonClicked"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogLogsUserActionWhenShown) {
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(GetProfile(), nullptr);

  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents.get(), /*enabled=*/false, base::DoNothing());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.DisabledDialog.Shown"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogHandleMessageAcceptedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing());
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "SafeBrowsing.AccountIntegration.EnabledDialog.OkButtonClicked"),
      1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogHandleMessageDismissedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing());
  DoMessageDismissed(&consented_modal, messages::DismissReason::TIMER);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.EnabledDialog.Dismissed"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogHandleSettingsClickedLogsUserAction) {
  // Create a scoped window so that WebContents::GetTopLevelNativeWindow does
  // not return null.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents_.get()->GetNativeView());
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing());
  DoSettingsClicked(&consented_modal);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("SafeBrowsing.AccountIntegration."
                                         "EnabledDialog.SettingsButtonClicked"),
      1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogLogsUserActionWhenShown) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.EnabledDialog.Shown"),
            1);
}

}  // namespace safe_browsing
