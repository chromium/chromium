// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/syslog_logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/metrics_utils.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

void RecordFailure(ManagementServiceError error,
                   const std::string& log_message) {
  RecordError(error);
  SYSLOG(ERROR) << log_message;
}

constexpr char kStableChannelHostName[] = "m.google.com";

// Returns decoded value from the base-64 `encoded_value`, or null
// in case of a decoding error. The returned value is an opaque binary
// blob and should not be treated as an ASCII or UTF-8 string.
absl::optional<std::string> Decode(const std::string& encoded_value) {
  if (encoded_value.empty())
    return absl::nullopt;

  std::string value;
  if (!base::Base64Decode(encoded_value, &value)) {
    RecordFailure(
        ManagementServiceError::kIncorrectlyEncodedArgument,
        "Argument passed on the command line is not correctly encoded.");
    return absl::nullopt;
  }
  return value;
}

}  // namespace

bool RotateDeviceTrustKey(
    std::unique_ptr<KeyRotationManager> key_rotation_manager,
    const base::CommandLine& command_line,
    version_info::Channel channel) {
  auto dm_token =
      Decode(command_line.GetSwitchValueASCII(switches::kRotateDTKey));
  if (!dm_token)
    return false;

  auto nonce = command_line.GetSwitchValueASCII(switches::kNonce);
  // The nonce command line argument is optional. If none is specified use
  // an empty string, however if one is specified we decode it.
  absl::optional<std::string> decoded_nonce;
  if (nonce.empty()) {
    decoded_nonce.emplace();
  } else {
    decoded_nonce = Decode(nonce);
    if (!decoded_nonce)
      return false;
  }

  if (!command_line.HasSwitch(switches::kDmServerUrl)) {
    RecordFailure(
        ManagementServiceError::kCommandMissingDMServerUrl,
        "Device trust key rotation failed. Command missing dm server url.");
    return false;
  }
  GURL dm_server_url(command_line.GetSwitchValueASCII(switches::kDmServerUrl));

  // An invalid command is when `channel` is stable and the `hostname` of
  // the dm server url is not a prod hostname.
  auto valid_command = (channel != version_info::Channel::STABLE ||
                        dm_server_url.host() == kStableChannelHostName);
  if (!valid_command || !dm_server_url.SchemeIsHTTPOrHTTPS()) {
    RecordFailure(
        ManagementServiceError::kInvalidRotateCommand,
        "Device trust key rotation failed. The server URL is invalid.");
    return false;
  }

  base::RunLoop run_loop;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  bool rotation_result = false;
  key_rotation_manager->Rotate(
      dm_server_url, *dm_token, *decoded_nonce,
      base::BindOnce(
          [](bool& rotation_result, base::OnceClosure quit_closure,
             KeyRotationManager::Result result) {
            rotation_result = (result == KeyRotationManager::Result::SUCCEEDED);
            std::move(quit_closure).Run();
          },
          std::ref(rotation_result), run_loop.QuitClosure()));
  run_loop.Run();
  return rotation_result;
}

}  // namespace enterprise_connectors
