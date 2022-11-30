// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_H_
#define CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_H_

#include <stddef.h>

#include <string>

namespace policy {

class ChromeBrowserCloudManagementBrowserTestDelegate {
 public:
  ChromeBrowserCloudManagementBrowserTestDelegate() = default;
  ~ChromeBrowserCloudManagementBrowserTestDelegate() = default;

  // Check if a notification dialog has been closed if |popup_expected| on
  // platforms that show an error message if policy registration fails.
  virtual void MaybeCheckDialogClosingAfterPolicyRegistration(
      bool popup_expected) const = 0;

  // Returns true on platforms in which policy manager initialization is not
  // deferred.
  virtual bool ExpectManagerImmediatelyInitialized(
      bool enrollment_succeeded) const = 0;

  // Returns true on platforms for which a machine name is not sent on browser
  // registration.
  virtual bool AcceptEmptyMachineNameOnBrowserRegistration() const = 0;

  // Returns true on platforms in which either OnStoreLoaded or OnStoreError
  // event should be fired even if the browser is not managed by CBCM.
  virtual bool ExpectOnStoreEventFired() const = 0;

  // Checks the number of active browsers on platforms where this is possible.
  virtual void MaybeCheckTotalBrowserCount(
      size_t expected_browser_count) const = 0;

  virtual void MaybeWaitForEnrollmentConfirmation(
      const std::string& enrollment_token) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_H_
