// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_utils.h"

#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_connectors {

absl::optional<std::string> GetUploadBrowserPublicKeyUrl(
    const std::string& client_id,
    const std::string& dm_token,
    policy::DeviceManagementService* device_management_service) {
  if (!device_management_service) {
    return absl::nullopt;
  }

  // Get the DM server URL to upload the public key. Reuse
  // DMServerJobConfiguration to reuse the URL building steps.
  policy::DMServerJobConfiguration config(
      device_management_service,
      policy::DeviceManagementService::JobConfiguration::
          TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
      client_id, true, policy::DMAuth::FromDMToken(dm_token), absl::nullopt,
      nullptr, base::DoNothing());

  auto resource_request = config.GetResourceRequest(false, 0);
  if (!resource_request) {
    return absl::nullopt;
  }

  if (!resource_request->url.is_valid()) {
    return absl::nullopt;
  }

  return resource_request->url.spec();
}

}  // namespace enterprise_connectors
