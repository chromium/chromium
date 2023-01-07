// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"

#include "build/build_config.h"
#include "chrome/common/chrome_version.h"

namespace enterprise_connectors {

namespace constants {

const char kBinaryFileName[] = "chrome-management-service";

const char kGroupName[] = "chromemgmt";

const char kSigningKeyFilePath[] = "enrollment/DeviceTrustSigningKey";

#if BUILDFLAG(IS_MAC)
const char kTemporaryDeviceTrustSigningKeyLabel[] =
    "GoogleChromeEnterpriseTempDTSigningKey";

const char kDeviceTrustSigningKeyLabel[] = "GoogleChromeEnterpriseDTSigningKey";

const char kKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".devicetrust";
#endif

}  // namespace constants

namespace switches {

const char kDmServerUrl[] = "dm-server-url";

const char kNonce[] = "nonce";

const char kPipeName[] = "pipe-name";

const char kRotateDTKey[] = "rotate-dtkey";

}  // namespace switches

}  // namespace enterprise_connectors
