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
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"

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

}  // namespace multi_capture
