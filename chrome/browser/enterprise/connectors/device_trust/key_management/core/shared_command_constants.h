// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SHARED_COMMAND_CONSTANTS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SHARED_COMMAND_CONSTANTS_H_

#include "build/build_config.h"

// Defines constants shared by both the browser and installers chrome
// management service executable.
namespace enterprise_connectors {

// Process exit codes of the chrome management service executable.
enum Status {
  // When the management-service successfully performed a key rotation.
  kSuccess = 0,

  // When the process does not return correctly. This is 1
  // because the WaitForExitWithTimeout method returns
  // false when the process does not correctly exit (e.g. crash).
  kUnknownFailure = 1,

  // When the management-service key rotation fails. This
  // differs from the kUnknownFailure because the process
  // correctly exited and returned a failure code.
  kFailure = 2,

  // Special error code returned when the browser is missing required
  // permissions.
  kFailedInsufficientPermissions = 3,

  // Special error code returned when a key upload resulted in a conflicting
  // HTTP response (409).
  kFailedKeyConflict = 4,
};

namespace constants {

// Specifies the filename of the chrome management service executable.
extern const char kBinaryFileName[];

// Specifies the group name that the chrome-management-service and the
// signing key file should have.
extern const char kGroupName[];

// Path to where the signing key is stored.
extern const char kSigningKeyFilePath[];

#if BUILDFLAG(IS_MAC)
// Temporary label for the secure enclave device trust signing key.
extern const char kTemporaryDeviceTrustSigningKeyLabel[];

// Permanent label for the secure enclave device trust signing key.
extern const char kDeviceTrustSigningKeyLabel[];

// The keychain-access-group for the secure enclave device trust signing key
// This allows all Chrome applications access to modify this key.
extern const char kKeychainAccessGroup[];
#endif

}  // namespace constants

namespace switches {

// Specifies the DM server URL to use with the rotate device key command.
extern const char kDmServerUrl[];

// Specifies a nonce to use with the rotate device key command.
extern const char kNonce[];

// Specifies the pipe name to connect to when accepting and sending mojo
// invitations.
extern const char kPipeName[];

// Rotate the stored device trust signing key.
extern const char kRotateDTKey[];

}  // namespace switches

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SHARED_COMMAND_CONSTANTS_H_
