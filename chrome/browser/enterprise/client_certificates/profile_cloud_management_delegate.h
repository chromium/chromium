// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

class Profile;

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_management {
class PolicyData;
}  // namespace enterprise_management

namespace client_certificates {

class ProfileCloudManagementDelegate : public CloudManagementDelegate {
 public:
  ProfileCloudManagementDelegate(
      Profile* profile,
      enterprise::ProfileIdService* profile_id_service,
      std::unique_ptr<DMServerClient> dmserver_client);

  ~ProfileCloudManagementDelegate() override;

  // CloudManagementDelegate:
  std::optional<std::string> GetDMToken() const override;

  void UploadBrowserPublicKey(
      const enterprise_management::DeviceManagementRequest& upload_request,
      policy::DMServerJobConfiguration::Callback callback) override;

 private:
  const enterprise_management::PolicyData* GetPolicyData() const;
  std::optional<std::string> GetClientID() const;

  const raw_ptr<Profile> profile_;
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  std::unique_ptr<client_certificates::DMServerClient> dmserver_client_;
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_
