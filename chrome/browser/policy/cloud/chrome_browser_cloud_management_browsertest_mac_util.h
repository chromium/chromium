// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_MAC_UTIL_H_
#define CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_MAC_UTIL_H_

namespace policy {

// This is a helper function for MachineLevelUserCloudPolicyEnrollmentTest. It
// sends few system notifications which app_controller_mac.mm listens to make
// sure they're handle properly when the EnterpriseStartupDialog is being
// displayed.
void PostAppControllerNSNotifications();

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_BROWSERTEST_MAC_UTIL_H_
