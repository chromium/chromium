// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"

class Profile;

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_management {
class PolicyData;
}  // namespace enterprise_management

namespace policy {
class DeviceManagementService;
}

namespace client_certificates {

class ProfileCloudManagementDelegate : public CloudManagementDelegate {
 public:
  ProfileCloudManagementDelegate(
      Profile* profile,
      policy::DeviceManagementService* device_management_service,
      enterprise::ProfileIdService* profile_id_service);
  ~ProfileCloudManagementDelegate() override;

  // CloudManagementDelegate:
  std::optional<std::string> GetDMToken() const override;
  std::optional<std::string> GetUploadBrowserPublicKeyUrl() const override;

 private:
  const enterprise_management::PolicyData* GetPolicyData() const;
  std::optional<std::string> GetClientID() const;

  const raw_ptr<Profile> profile_;
  const raw_ptr<policy::DeviceManagementService> device_management_service_;
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_
