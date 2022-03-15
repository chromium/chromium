// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kStableChannelHostName[] = "m.google.com";

// Returns decoded value from the base-64 `encoded_value`, or null
// in case of a decoding error. The returned value is an opaque binary
// blob and should not be treated as an ASCII or UTF-8 string.
absl::optional<std::string> Decode(const std::string& encoded_value) {
  if (encoded_value.empty())
    return absl::nullopt;

  std::string value;
  if (!base::Base64Decode(encoded_value, &value)) {
    SYSLOG(ERROR)
        << "Argument passed on the command line is not correctly encoded";
    return absl::nullopt;
  }
  return value;
}

}  // namespace

bool RotateDeviceTrustKey(
    std::unique_ptr<KeyRotationManager> key_rotation_manager,
    base::CommandLine* command_line,
    version_info::Channel channel) {
  auto dm_token =
      Decode(command_line->GetSwitchValueASCII(switches::kRotateDTKey));

  auto nonce = command_line->GetSwitchValueASCII(switches::kNonce);
  // The nonce command line argument is optional. If none is specified use
  // an empty string, however if one is specified we decoded it.
  absl::optional<std::string> decoded_nonce;
  nonce.empty() ? decoded_nonce = std::string() : decoded_nonce = Decode(nonce);
  if (!dm_token || !decoded_nonce)
    return false;

  GURL dm_server_url(command_line->GetSwitchValueASCII(switches::kDmServerUrl));

  // an invalid command is when `channel` is stable and the `hostname` of
  // the dm server url is not a prod hostname.
  auto valid_command = (channel != version_info::Channel::STABLE ||
                        dm_server_url.host() == kStableChannelHostName);
  if (!valid_command || !dm_server_url.SchemeIsHTTPOrHTTPS()) {
    SYSLOG(ERROR)
        << "Device trust key rotation failed. Invalid command to rotate.";
    return false;
  }

  return key_rotation_manager->RotateWithAdminRights(dm_server_url, *dm_token,
                                                     *decoded_nonce);
}

}  // namespace enterprise_connectors
