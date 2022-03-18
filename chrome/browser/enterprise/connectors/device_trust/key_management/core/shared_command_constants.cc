// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"

namespace enterprise_connectors {

namespace constants {

const char kBinaryFileName[] = "chrome-management-service";

const char kGroupName[] = "chromemgmt";

const char kSigningKeyFilePath[] = "enrollment/DeviceTrustSigningKey";

}  // namespace constants

namespace switches {

const char kDmServerUrl[] = "dm-server-url";

const char kNonce[] = "nonce";

const char kPipeName[] = "pipe-name";

const char kRotateDTKey[] = "rotate-dtkey";

}  // namespace switches

}  // namespace enterprise_connectors
