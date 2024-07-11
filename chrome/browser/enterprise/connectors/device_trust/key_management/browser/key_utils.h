// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_UTILS_H_

#include <optional>
#include <string>

namespace policy {
class DeviceManagementService;
}  // namespace policy

// TODO(b/351201459): Remove this entirely, when
// DTCRetryUploadingPublicKeyEnabled is fully launched.
namespace enterprise_connectors {

// Given the `client_id`, 'dm_token', `profile_id` and
// `device_management_service`, this returns the DM server URL.
std::optional<std::string> GetUploadBrowserPublicKeyUrl(
    const std::string& client_id,
    const std::string& dm_token,
    const std::optional<std::string>& profile_id,
    policy::DeviceManagementService* device_management_service);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_UTILS_H_
