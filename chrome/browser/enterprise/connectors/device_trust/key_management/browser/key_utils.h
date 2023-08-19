// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_UTILS_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

// Given the`client_id`, 'dm_token' and `device_management_service`, this
// returns the DM server URL.
absl::optional<std::string> GetUploadBrowserPublicKeyUrl(
    const std::string& client_id,
    const std::string& dm_token,
    policy::DeviceManagementService* device_management_service);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_UTILS_H_
