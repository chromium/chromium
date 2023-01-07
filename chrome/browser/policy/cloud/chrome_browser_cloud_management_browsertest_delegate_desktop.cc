// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate_desktop.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace policy {

void ChromeBrowserCloudManagementBrowserTestDelegateDesktop::
    MaybeCheckDialogClosingAfterPolicyRegistration(bool popup_expected) const {
  if (popup_expected) {
    MaybeCheckTotalBrowserCount(0u);
#if BUILDFLAG(IS_MAC)
    PostAppControllerNSNotifications();
#endif
    // Close the error dialog.
    ASSERT_EQ(1u, views::test::WidgetTest::GetAllWidgets().size());  // IN-TEST
    (*views::test::WidgetTest::GetAllWidgets().begin())->Close();    // IN-TEST
  }
}

bool ChromeBrowserCloudManagementBrowserTestDelegateDesktop::
    ExpectManagerImmediatelyInitialized(bool enrollment_succeeded) const {
  // If enrollment fails, the manager should be marked as initialized
  // immediately. Otherwise, this will be done after the policy data is
  // downloaded.
  return !enrollment_succeeded;
}

bool ChromeBrowserCloudManagementBrowserTestDelegateDesktop::
    AcceptEmptyMachineNameOnBrowserRegistration() const {
  return false;
}

bool ChromeBrowserCloudManagementBrowserTestDelegateDesktop::
    ExpectOnStoreEventFired() const {
  return false;
}

void ChromeBrowserCloudManagementBrowserTestDelegateDesktop::
    MaybeCheckTotalBrowserCount(size_t expected_browser_count) const {
  EXPECT_EQ(chrome::GetTotalBrowserCount(), expected_browser_count);
}

void ChromeBrowserCloudManagementBrowserTestDelegateDesktop::
    MaybeWaitForEnrollmentConfirmation(const std::string& enrollment_token) {}

}  // namespace policy
