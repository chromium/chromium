// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

// static
bool LaunchKeyRotation(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    const std::string& nonce) {
  DCHECK(dm_token_storage);
  DCHECK(device_management_service);
  if (!dm_token_storage || !device_management_service) {
    return false;
  }

  // Get the CBCM DM token.  This will be needed later to send the new key's
  // public part to the server.
  auto client_id = dm_token_storage->RetrieveClientId();
  auto dm_token = dm_token_storage->RetrieveDMToken();

  if (!dm_token.is_valid())
    return false;

  // Get the DM server URL to upload the public key.  Reuse
  // DMServerJobConfiguration to reuse the URL building steps.
  policy::DMServerJobConfiguration config(
      device_management_service,
      policy::DeviceManagementService::JobConfiguration::
          TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
      client_id, true, policy::DMAuth::FromDMToken(dm_token.value()),
      absl::nullopt, nullptr, base::DoNothing());
  std::string dm_server_url = config.GetResourceRequest(false, 0)->url.spec();

  KeyRotationCommand::Params params{dm_token.value(), dm_server_url, nonce};
  return KeyRotationCommandFactory::GetInstance()->CreateCommand()->Trigger(
      params);
}

}  // namespace enterprise_connectors
