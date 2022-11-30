// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ANDROID_CLOUD_MANAGEMENT_ANDROID_CONNECTION_H_
#define CHROME_BROWSER_POLICY_ANDROID_CLOUD_MANAGEMENT_ANDROID_CONNECTION_H_

#include <string>

namespace policy {
namespace android {

// Returns the client ID to be used in the DM token generation.
std::string GetClientId();

}  // namespace android
}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ANDROID_CLOUD_MANAGEMENT_ANDROID_CONNECTION_H_
