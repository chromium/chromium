// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate.h"

namespace policy {

void ChromeBrowserCloudManagementBrowserTestDelegate::
    MaybeCheckDialogClosingAfterPolicyRegistration(bool popup_expected) const {
  // There is no popup shown on Android, no need to check.
}

bool ChromeBrowserCloudManagementBrowserTestDelegate::
    ExpectManagerImmediatelyInitialized(bool enrollment_succeeded) const {
  // Manager is always initialized immediately on Android, so it unblocks
  // startup routines that wait for policy service to be available (e.g. FRE).
  //
  // TODO(http://crbug.com/1203435): this may no longer be needed now that we
  // use ProxyPolicyProvider as an indirection to the manager.
  return true;
}

bool ChromeBrowserCloudManagementBrowserTestDelegate::
    AcceptEmptyMachineNameOnBrowserRegistration() const {
  return true;
}

bool ChromeBrowserCloudManagementBrowserTestDelegate::ExpectOnStoreEventFired()
    const {
  return true;
}

void ChromeBrowserCloudManagementBrowserTestDelegate::
    MaybeCheckTotalBrowserCount(size_t expected_browser_count) const {
  // No checks needed, as enrollment is never required on Android.
}

}  // namespace policy
