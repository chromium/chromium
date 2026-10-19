// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_session_controller.h"

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_session_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"

namespace multi_capture {

using MultiCaptureSessionControllerBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       StopsCaptureOnActiveUserChanged) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);

  base::test::TestFuture<void> stop_future;
  controller->MultiCaptureStarted("test_label", stop_future.GetCallback());

  // Simulate active user changing.
  controller->ActiveUserChanged(nullptr);

  // The stop_callback should have been called.
  EXPECT_TRUE(stop_future.Wait());
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       DoesNotStopCaptureIfStoppedNormally) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);

  bool was_called = false;
  auto stop_callback =
      base::BindOnce([](bool* called) { *called = true; }, &was_called);

  controller->MultiCaptureStarted("test_label", std::move(stop_callback));
  controller->MultiCaptureStopped("test_label");

  // Simulate active user changing.
  controller->ActiveUserChanged(nullptr);

  // The stop_callback should not have been called, because it was erased
  // when MultiCaptureStopped was called.
  EXPECT_FALSE(was_called);
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       StopsCaptureOnScreenLocked) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);

  base::test::TestFuture<void> stop_future;
  controller->MultiCaptureStarted("test_label", stop_future.GetCallback());

  // Simulate screen locking.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  controller->OnSessionStateChanged();

  // The stop_callback should have been called.
  EXPECT_TRUE(stop_future.Wait());
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       StopsCaptureOnLoginSecondary) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);

  base::test::TestFuture<void> stop_future;
  controller->MultiCaptureStarted("test_label", stop_future.GetCallback());

  // Simulate secondary login.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  controller->OnSessionStateChanged();

  // The stop_callback should have been called.
  EXPECT_TRUE(stop_future.Wait());
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       StopsCaptureOnLoginPrimary) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);

  base::test::TestFuture<void> stop_future;
  controller->MultiCaptureStarted("test_label", stop_future.GetCallback());

  // Simulate primary login.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  controller->OnSessionStateChanged();

  // The stop_callback should have been called.
  EXPECT_TRUE(stop_future.Wait());
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       StopsCaptureOnLoggedInNotActive) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);

  base::test::TestFuture<void> stop_future;
  controller->MultiCaptureStarted("test_label", stop_future.GetCallback());

  // Simulate transient LOGGED_IN_NOT_ACTIVE state during secondary sign-in.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  // The stop_callback should have been called.
  EXPECT_TRUE(stop_future.Wait());
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       DisallowsCaptureWhenSessionNotActive) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsSessionEligibleForMultiCapture());

  for (const auto blocked_state :
       {session_manager::SessionState::LOCKED,
        session_manager::SessionState::LOGIN_PRIMARY,
        session_manager::SessionState::LOGIN_SECONDARY,
        session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
        session_manager::SessionState::OOBE,
        session_manager::SessionState::UNKNOWN,
        session_manager::SessionState::RMA}) {
    session_manager::SessionManager::Get()->SetSessionState(blocked_state);
    EXPECT_FALSE(controller->IsSessionEligibleForMultiCapture());
  }

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(controller->IsSessionEligibleForMultiCapture());
}

IN_PROC_BROWSER_TEST_F(MultiCaptureSessionControllerBrowserTest,
                       DisallowsCaptureWhenUserNotActive) {
  MultiCaptureSessionController* controller =
      MultiCaptureSessionControllerFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsSessionEligibleForMultiCapture());

  const AccountId secondary_account_id = AccountId::FromUserEmailGaiaId(
      "secondary@example.com", GaiaId("secondary-gaia-id"));

  user_manager::UserManager::Get()->EnsureUser(secondary_account_id,
                                               user_manager::UserType::kRegular,
                                               /*is_ephemeral=*/false);
  session_manager::SessionManager::Get()->CreateSession(
      secondary_account_id,
      user_manager::TestHelper::GetFakeUsernameHash(secondary_account_id),
      /*new_user=*/false, /*has_active_session=*/true);

  // Switch active user to the secondary user while SessionState remains ACTIVE.
  session_manager::SessionManager::Get()->SwitchActiveSession(
      secondary_account_id);
  EXPECT_FALSE(controller->IsSessionEligibleForMultiCapture());
}

}  // namespace multi_capture
