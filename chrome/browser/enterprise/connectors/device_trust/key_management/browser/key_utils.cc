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

std::optional<std::string> GetUploadBrowserPublicKeyUrl(
    const std::string& client_id,
    const std::string& dm_token,
    const std::optional<std::string>& profile_id,
    policy::DeviceManagementService* device_management_service) {
  if (!device_management_service) {
    return std::nullopt;
  }

  auto params = policy::DMServerJobConfiguration::CreateParams::WithoutClient(
      policy::DeviceManagementService::JobConfiguration::
          TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
      device_management_service, client_id, nullptr);

  params.critical = true;
  params.auth_data = policy::DMAuth::FromDMToken(dm_token);
  if (profile_id.has_value()) {
    params.profile_id = profile_id.value();
  }

  policy::DMServerJobConfiguration config(std::move(params));

  auto resource_request = config.GetResourceRequest(false, 0);
  if (!resource_request) {
    return std::nullopt;
  }

  if (!resource_request->url.is_valid()) {
    return std::nullopt;
  }

  return resource_request->url.spec();
}

}  // namespace enterprise_connectors
