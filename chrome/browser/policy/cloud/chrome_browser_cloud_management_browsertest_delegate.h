// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_H_
#define CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_H_

#include <stddef.h>

namespace policy {

class ChromeBrowserCloudManagementBrowserTestDelegate {
 public:
  ChromeBrowserCloudManagementBrowserTestDelegate() = default;
  ~ChromeBrowserCloudManagementBrowserTestDelegate() = default;

  // Check if a notification dialog has been closed if |popup_expected| on
  // platforms that show an error message if policy registration fails.
  void MaybeCheckDialogClosingAfterPolicyRegistration(
      bool popup_expected) const;

  // Returns true on platforms in which policy manager initialization is not
  // deferred.
  bool ExpectManagerImmediatelyInitialized(bool enrollment_succeeded) const;

  // Returns true on platforms for which a machine name is not sent on browser
  // registration.
  bool AcceptEmptyMachineNameOnBrowserRegistration() const;

  // Returns true on platforms in which either OnStoreLoaded or OnStoreError
  // event should be fired even if the browser is not managed by CBCM.
  bool ExpectOnStoreEventFired() const;

  // Checks the number of active browsers on platforms where this is possible.
  void MaybeCheckTotalBrowserCount(size_t expected_browser_count) const;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_DELEGATE_H_
