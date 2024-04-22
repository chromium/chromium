// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/profile_cloud_management_delegate.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace client_certificates {

ProfileCloudManagementDelegate::ProfileCloudManagementDelegate(
    Profile* profile,
    policy::DeviceManagementService* device_management_service,
    enterprise::ProfileIdService* profile_id_service)
    : profile_(profile),
      device_management_service_(device_management_service),
      profile_id_service_(profile_id_service) {
  CHECK(profile_);
  CHECK(device_management_service_);
  CHECK(profile_id_service_);
}

ProfileCloudManagementDelegate::~ProfileCloudManagementDelegate() = default;

std::optional<std::string> ProfileCloudManagementDelegate::GetDMToken() const {
  const auto* policy_data = GetPolicyData();
  if (!policy_data || !policy_data->has_request_token()) {
    return std::nullopt;
  }
  return policy_data->request_token();
}

std::optional<std::string>
ProfileCloudManagementDelegate::GetUploadBrowserPublicKeyUrl() const {
  auto dm_token = GetDMToken();
  auto client_id = GetClientID();
  if (!dm_token.has_value() || !client_id.has_value()) {
    return std::nullopt;
  }

  return enterprise_connectors::GetUploadBrowserPublicKeyUrl(
      client_id.value(), dm_token.value(), profile_id_service_->GetProfileId(),
      device_management_service_);
}

const enterprise_management::PolicyData*
ProfileCloudManagementDelegate::GetPolicyData() const {
  policy::CloudPolicyManager* policy_manager =
      profile_->GetCloudPolicyManager();
  if (policy_manager && policy_manager->core() &&
      policy_manager->core()->store() &&
      policy_manager->core()->store()->has_policy()) {
    return policy_manager->core()->store()->policy();
  }
  return nullptr;
}

std::optional<std::string> ProfileCloudManagementDelegate::GetClientID() const {
  const auto* policy_data = GetPolicyData();
  if (!policy_data || !policy_data->has_device_id()) {
    return std::nullopt;
  }
  return policy_data->device_id();
}

}  // namespace client_certificates
