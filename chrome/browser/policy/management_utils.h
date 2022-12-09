// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MANAGEMENT_UTILS_H_
#define CHROME_BROWSER_POLICY_MANAGEMENT_UTILS_H_

namespace policy {

// Returns whether the device is enterprise managed. Note that on Linux, there's
// no good way of detecting whether the device is managed, so always return
// false.
bool IsDeviceEnterpriseManaged();

// Returns true if the device is managed by a cloud source.
bool IsDeviceCloudManaged();

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MANAGEMENT_UTILS_H_
