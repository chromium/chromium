// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature_channel.h"

namespace ash {

namespace {

void EnableSpokenFeedback() {
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
}

bool IsSpokenFeedbackEnabled() {
  return AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

void EnableSwitchAccess() {
  AccessibilityManager::Get()->SetSwitchAccessEnabled(true);
}

bool IsSwitchAccessEnabled() {
  return AccessibilityManager::Get()->IsSwitchAccessEnabled();
}

void EnableSelectToSpeak() {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
}

bool IsSelectToSpeakEnabled() {
  return AccessibilityManager::Get()->IsSelectToSpeakEnabled();
}

}  // namespace

class AccessibilityExtensionChannelTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<version_info::Channel> {
 protected:
  AccessibilityExtensionChannelTest() : channel_(GetParam()) {}
  ~AccessibilityExtensionChannelTest() = default;
  AccessibilityExtensionChannelTest(const AccessibilityExtensionChannelTest&) =
      delete;
  AccessibilityExtensionChannelTest& operator=(
      const AccessibilityExtensionChannelTest&) = delete;

  void LoadExtensionAndPerformChecks(
      const char* extension_id,
      base::OnceCallback<void()> enable_extension,
      base::OnceCallback<bool()> is_extension_enabled) {
    ExtensionConsoleErrorObserver console_observer(browser()->profile(),
                                                   extension_id);
    extensions::ExtensionHostTestHelper host_helper(browser()->profile(),
                                                    extension_id);
    std::move(enable_extension).Run();
    host_helper.WaitForHostCompletedFirstLoad();
    EXPECT_TRUE(std::move(is_extension_enabled).Run());
    EXPECT_FALSE(console_observer.HasErrorsOrWarnings())
        << "Found console.warn or console.error with message: "
        << console_observer.GetErrorOrWarningAt(0);
  }

 private:
  extensions::ScopedCurrentChannel channel_;
};

INSTANTIATE_TEST_SUITE_P(Channels,
                         AccessibilityExtensionChannelTest,
                         testing::Values(version_info::Channel::STABLE,
                                         version_info::Channel::BETA,
                                         version_info::Channel::DEV,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEFAULT));

IN_PROC_BROWSER_TEST_P(AccessibilityExtensionChannelTest, ChromeVox) {
  LoadExtensionAndPerformChecks(extension_misc::kChromeVoxExtensionId,
                                base::BindOnce(&EnableSpokenFeedback),
                                base::BindOnce(&IsSpokenFeedbackEnabled));
}

IN_PROC_BROWSER_TEST_P(AccessibilityExtensionChannelTest, SwitchAccess) {
  LoadExtensionAndPerformChecks(extension_misc::kSwitchAccessExtensionId,
                                base::BindOnce(&EnableSwitchAccess),
                                base::BindOnce(&IsSwitchAccessEnabled));
}

IN_PROC_BROWSER_TEST_P(AccessibilityExtensionChannelTest, SelectToSpeak) {
  LoadExtensionAndPerformChecks(extension_misc::kSelectToSpeakExtensionId,
                                base::BindOnce(&EnableSelectToSpeak),
                                base::BindOnce(&IsSelectToSpeakEnabled));
}

}  // namespace ash
