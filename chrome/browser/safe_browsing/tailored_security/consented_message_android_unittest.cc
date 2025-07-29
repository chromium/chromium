// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace safe_browsing {
constexpr int kSyncedEsbOutcomeAcceptedFailed = 4;

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
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogHandleMessageAcceptedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "SafeBrowsing.AccountIntegration.DisabledDialog.OkButtonClicked"),
      1);
}

TEST_F(
    TailoredSecurityConsentedModalAndroidTest,
    DisabledDialogHandleMessageAcceptedSyncEsbTurnOnButtonClickLogsUserAction) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kEsbAsASyncedSetting);
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.SyncedEsbDialog.TurnOnButtonClicked"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogHandleMessageAcceptedSyncEsbOkButtonClickLogsUserAction) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kEsbAsASyncedSetting);
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.SyncedEsbDialog.OkButtonClicked"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       DisabledDialogHandleMessageDismissedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
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
      web_contents_.get(), /*enabled=*/false, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
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
      web_contents.get(), /*enabled=*/false, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.DisabledDialog.Shown"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogHandleMessageAcceptedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "SafeBrowsing.AccountIntegration.EnabledDialog.OkButtonClicked"),
      1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogHandleMessageAcceptedSyncEsbOkButtonClickLogsUserAction) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kEsbAsASyncedSetting);
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  DoMessageAccepted(&consented_modal);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.SyncedEsbDialog.OkButtonClicked"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogHandleMessageDismissedLogsUserAction) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
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
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  DoSettingsClicked(&consented_modal);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("SafeBrowsing.AccountIntegration."
                                         "EnabledDialog.SettingsButtonClicked"),
      1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       EnabledDialogLogsUserActionWhenShown) {
  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SafeBrowsing.AccountIntegration.EnabledDialog.Shown"),
            1);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       HandleMessageDismissedWithSelfDeletingCallback) {
  // Create the ScopedWindowAndroidForTesting wrapper.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      scoped_window_wrapper = ui::WindowAndroid::CreateForTesting();
  ASSERT_TRUE(scoped_window_wrapper);

  // Get the actual WindowAndroid* from the wrapper.
  ui::WindowAndroid* actual_window_android = scoped_window_wrapper->get();
  ASSERT_TRUE(actual_window_android);
  ASSERT_TRUE(web_contents_.get());
  ui::ViewAndroid* web_contents_view = web_contents_->GetNativeView();
  ASSERT_TRUE(web_contents_view);

  // Call AddChild on the actual WindowAndroid instance
  actual_window_android->AddChild(web_contents_view);

  EXPECT_CALL(message_dispatcher_bridge_,
              EnqueueWindowScopedMessage(testing::_, actual_window_android,
                                         testing::_));
  auto modal_ptr = std::make_unique<TailoredSecurityConsentedModalAndroid>(
      web_contents_.get(), /*enable=*/true,
      base::DoNothing(),  // This will be overwritten
      /*is_requested_by_synced_esb=*/false);

  auto* modal_raw_ptr = modal_ptr.get();
  modal_raw_ptr->dismiss_callback_ = base::BindOnce(
      [](std::unique_ptr<TailoredSecurityConsentedModalAndroid>
             modal_to_delete) {
        modal_to_delete.reset();  // This triggers the destructor
      },
      std::move(modal_ptr));

  DoMessageDismissed(modal_raw_ptr, messages::DismissReason::TIMER);

  SUCCEED();  // If it reaches here without crashing, the test passes.
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       HandleAccepted_EsbSynced_NullWebContents_EnabledMessage_LogsFailed) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kEsbAsASyncedSetting);

  // The modal constructor needs a valid WebContents and an associated window.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  ASSERT_TRUE(window && window->get() && web_contents_ &&
              web_contents_->GetNativeView());
  window->get()->AddChild(web_contents_->GetNativeView());

  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);

  consented_modal.web_contents_ = nullptr;

  base::HistogramTester histogram_tester;
  DoMessageAccepted(&consented_modal);

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SyncedEsbDialogEnabledMessageOutcome",
      kSyncedEsbOutcomeAcceptedFailed, 1);
  // Ensure the other related histogram is not affected.
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SyncedEsbDialogDisabledMessageOutcome", 0);
}

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       HandleAccepted_EsbSynced_NullWebContents_DisabledMessage_LogsFailed) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kEsbAsASyncedSetting);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  ASSERT_TRUE(window && window->get() && web_contents_ &&
              web_contents_->GetNativeView())
      << "Test setup failed: window or web_contents issue.";
  window->get()->AddChild(web_contents_->GetNativeView());

  TailoredSecurityConsentedModalAndroid consented_modal(
      web_contents_.get(), /*enabled=*/false, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);

  consented_modal.web_contents_ = nullptr;

  base::HistogramTester histogram_tester;
  DoMessageAccepted(&consented_modal);

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SyncedEsbDialogDisabledMessageOutcome",
      kSyncedEsbOutcomeAcceptedFailed, 1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SyncedEsbDialogEnabledMessageOutcome", 0);
}

class TestTailoredSecurityConsentedModal
    : public TailoredSecurityConsentedModalAndroid {
 public:
  using TailoredSecurityConsentedModalAndroid::
      TailoredSecurityConsentedModalAndroid;

  // Set a callback to be run when the WebContents is destroyed.
  void SetWebContentsDestroyedClosure(base::OnceClosure closure) {
    web_contents_destroyed_closure_ = std::move(closure);
  }

  // Override the observer method to run the callback.
  void WebContentsDestroyed() override {
    // Call the original base class implementation first.
    TailoredSecurityConsentedModalAndroid::WebContentsDestroyed();
    // Run test-specific callback to signal the RunLoop.
    if (web_contents_destroyed_closure_) {
      std::move(web_contents_destroyed_closure_).Run();
    }
  }

 private:
  base::OnceClosure web_contents_destroyed_closure_;
};

TEST_F(TailoredSecurityConsentedModalAndroidTest,
       WebContentsDestroyedNullifiesPointer) {
  base::RunLoop run_loop;

  auto modal = std::make_unique<TestTailoredSecurityConsentedModal>(
      web_contents_.get(), /*enable=*/true, base::DoNothing(),
      /*is_requested_by_synced_esb=*/false);

  modal->SetWebContentsDestroyedClosure(run_loop.QuitClosure());
  // Destroy the WebContents, which will trigger the notification.
  web_contents_.reset();

  // Run the loop. This will block until the QuitClosure is called from
  // within WebContentsDestroyed().
  run_loop.Run();
  EXPECT_EQ(nullptr, modal->web_contents_);
}

}  // namespace safe_browsing
