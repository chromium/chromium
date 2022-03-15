// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SHARED_COMMAND_CONSTANTS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SHARED_COMMAND_CONSTANTS_H_

// Defines constants shared by both the browser and installers chrome
// management service executable.
namespace enterprise_connectors {

// Process exit codes of the chrome management service executable.
enum Status {
  kSuccess = 0,
  kFailure = 1,
};

namespace constants {

// Specifies the filename of the chrome management service executable.
extern const char kBinaryFileName[];

// Path to where the signing key is stored.
extern const char kSigningKeyFilePath[];

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
