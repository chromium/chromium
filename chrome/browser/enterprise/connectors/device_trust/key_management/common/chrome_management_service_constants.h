// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines all constants related to the chrome management service process.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_COMMON_CHROME_MANAGEMENT_SERVICE_CONSTANTS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_COMMON_CHROME_MANAGEMENT_SERVICE_CONSTANTS_H_

namespace chrome_management_service {

// Process exit codes of the chrome management service executable.
enum Status {
  kSuccess = 0,           // Successfully executed the key rotation.
  kStoreKeyFailure = 1,   // Writing to signing key storage failed.
  kUploadKeyFailure = 2,  // Sending the public key to the DM server failed.
  kInstanceAlreadyRunning = 3,  // Another instance of service is running.
  kInvalidHostName = 4,  // The hostname for dm_server_url on a stable channel
                         // is not a prod hostname.
};

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

}  // namespace chrome_management_service

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_COMMON_CHROME_MANAGEMENT_SERVICE_CONSTANTS_H_
