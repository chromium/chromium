// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/eche_app/eche_app_accessibility_provider_proxy.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::eche_app {
class EcheAppAccessibilityProviderProxyBrowserTest
    : public InProcessBrowserTest {
 protected:
  EcheAppAccessibilityProviderProxyBrowserTest() = default;
  EcheAppAccessibilityProviderProxyBrowserTest(
      const EcheAppAccessibilityProviderProxyBrowserTest&) = delete;
  EcheAppAccessibilityProviderProxyBrowserTest& operator=(
      const EcheAppAccessibilityProviderProxyBrowserTest&) = delete;

  ~EcheAppAccessibilityProviderProxyBrowserTest() override = default;

  void SetUpOnMainThread() override {
    a11y_manager_ = AccessibilityManager::Get();
  }

  void TearDownOnMainThread() override { a11y_manager_ = nullptr; }

  raw_ptr<AccessibilityManager> a11y_manager_;
  EcheAppAccessibilityProviderProxy proxy_;
};

IN_PROC_BROWSER_TEST_F(EcheAppAccessibilityProviderProxyBrowserTest,
                       EnabledState) {
  // Tell the proxy to register subscription to a11y_manager.
  proxy_.OnViewTracked();
  // Create a callback for a11y being enabled or disabled.
  bool current_state = false;
  proxy_.SetAccessibilityEnabledStateChangedCallback(base::BindRepeating(
      [](bool* local_state, bool state) { *local_state = state; },
      &current_state));
  // Enable ChromeVox.
  a11y_manager_->EnableSpokenFeedback(true);
  // Expect the current state to be true.
  EXPECT_TRUE(current_state);
  EXPECT_TRUE(proxy_.IsAccessibilityEnabled());
}

IN_PROC_BROWSER_TEST_F(EcheAppAccessibilityProviderProxyBrowserTest,
                       ExploreByTouchNotAlreadyEnabled) {
  bool current_state = false;
  proxy_.SetExploreByTouchEnabledStateChangedCallback(base::BindRepeating(
      [](bool* local_state, bool state) { *local_state = state; },
      &current_state));
  proxy_.OnViewTracked();
  // Explore by touch should not have been updated.
  EXPECT_FALSE(current_state);
  a11y_manager_->EnableSpokenFeedback(true);
  EXPECT_TRUE(current_state);
  a11y_manager_->EnableSpokenFeedback(false);
  EXPECT_FALSE(current_state);
}

IN_PROC_BROWSER_TEST_F(EcheAppAccessibilityProviderProxyBrowserTest,
                       ExploreByTouchAlreadyEnabled) {
  // Enable ChromeVox before setting up proxy.
  a11y_manager_->EnableSpokenFeedback(true);
  bool current_state = false;
  proxy_.SetExploreByTouchEnabledStateChangedCallback(base::BindRepeating(
      [](bool* local_state, bool state) { *local_state = state; },
      &current_state));
  proxy_.OnViewTracked();
  // Explore by touch should be updated when the view is tracked.
  EXPECT_TRUE(current_state);
  a11y_manager_->EnableSpokenFeedback(false);
  EXPECT_FALSE(current_state);
}

IN_PROC_BROWSER_TEST_F(EcheAppAccessibilityProviderProxyBrowserTest,
                       FullFocusMode) {
  proxy_.OnViewTracked();

  // ChromeVox
  EXPECT_FALSE(proxy_.UseFullFocusMode());
  a11y_manager_->EnableSpokenFeedback(true);
  EXPECT_TRUE(proxy_.UseFullFocusMode());
  a11y_manager_->EnableSpokenFeedback(false);
  EXPECT_FALSE(proxy_.UseFullFocusMode());

  // Switch Access
  EXPECT_FALSE(proxy_.UseFullFocusMode());
  a11y_manager_->SetSwitchAccessEnabled(true);
  EXPECT_TRUE(proxy_.UseFullFocusMode());
  a11y_manager_->SetSwitchAccessEnabled(false);
  EXPECT_FALSE(proxy_.UseFullFocusMode());

  // Something else

  EXPECT_FALSE(proxy_.UseFullFocusMode());
  a11y_manager_->SetSelectToSpeakEnabled(true);
  EXPECT_FALSE(proxy_.UseFullFocusMode());
  a11y_manager_->SetSwitchAccessEnabled(false);
  EXPECT_FALSE(proxy_.UseFullFocusMode());
}

}  // namespace ash::eche_app
