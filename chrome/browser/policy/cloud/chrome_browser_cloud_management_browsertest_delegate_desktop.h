// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_DESKTOP_H_

#include <stddef.h>

#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate.h"

namespace policy {

class ChromeBrowserCloudManagementBrowserTestDelegateDesktop
    : public ChromeBrowserCloudManagementBrowserTestDelegate {
 public:
  ChromeBrowserCloudManagementBrowserTestDelegateDesktop() = default;
  ~ChromeBrowserCloudManagementBrowserTestDelegateDesktop() = default;

  // ChromeBrowserCloudManagementBrowserTestDelegate:
  void MaybeCheckDialogClosingAfterPolicyRegistration(
      bool popup_expected) const override;
  bool ExpectManagerImmediatelyInitialized(
      bool enrollment_succeeded) const override;
  bool AcceptEmptyMachineNameOnBrowserRegistration() const override;
  bool ExpectOnStoreEventFired() const override;
  void MaybeCheckTotalBrowserCount(
      size_t expected_browser_count) const override;
  void MaybeWaitForEnrollmentConfirmation(
      const std::string& enrollment_token) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_DESKTOP_H_
