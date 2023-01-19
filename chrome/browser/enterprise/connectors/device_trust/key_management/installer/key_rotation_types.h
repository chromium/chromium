// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_TYPES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_TYPES_H_

namespace enterprise_connectors {

//  Status of the key rotation.
enum class KeyRotationResult {
  kSucceeded,
  kFailed,
  kInsufficientPermissions,
  kFailedKeyConflict,
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_KEY_ROTATION_TYPES_H_
