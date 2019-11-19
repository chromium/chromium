// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_HELPER_H_
#define CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_HELPER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudPolicyClient;
class CloudPolicyClientRegistrationHelper;
class MachineLevelUserCloudPolicyManager;
class DeviceManagementService;

// A helper class that register device with the enrollment token and client id.
class ChromeBrowserCloudManagementRegistrar {
 public:
  ChromeBrowserCloudManagementRegistrar(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ChromeBrowserCloudManagementRegistrar();

  // The callback invoked once policy registration is complete. Passed
  // |dm_token| and |client_id| parameters are empty if policy registration
  // failed.
  // TODO(crbug.com/825321): Update this to OnceCallback.
  using CloudManagementRegistrationCallback =
      base::RepeatingCallback<void(const std::string& dm_token,
                                   const std::string& client_id)>;

  // Registers a CloudPolicyClient for fetching machine level user policy.
  void RegisterForCloudManagementWithEnrollmentToken(
      const std::string& enrollment_token,
      const std::string& client_id,
      const CloudManagementRegistrationCallback& callback);

 private:
  void CallCloudManagementRegistrationCallback(
      std::unique_ptr<CloudPolicyClient> client,
      CloudManagementRegistrationCallback callback);

  std::unique_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;
  DeviceManagementService* device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserCloudManagementRegistrar);
};

// A helper class that setup registration and fetch policy.
class MachineLevelUserCloudPolicyFetcher : public CloudPolicyService::Observer {
 public:
  MachineLevelUserCloudPolicyFetcher(
      MachineLevelUserCloudPolicyManager* policy_manager,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~MachineLevelUserCloudPolicyFetcher() override;

  // Initialize the cloud policy client and policy store then fetch
  // the policy based on the |dm_token|. It should be called only once.
  void SetupRegistrationAndFetchPolicy(const std::string& dm_token,
                                       const std::string& client_id);

  // CloudPolicyService::Observer:
  void OnCloudPolicyServiceInitializationCompleted() override;

 private:
  void InitializeManager(std::unique_ptr<CloudPolicyClient> client);
  // Fetch policy if device is enrolled.
  void TryToFetchPolicy();

  MachineLevelUserCloudPolicyManager* policy_manager_;
  PrefService* local_state_;
  DeviceManagementService* device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyFetcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_HELPER_H_
