// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ANDROID_MANAGEMENT_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ANDROID_MANAGEMENT_CLIENT_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "google_apis/gaia/core_account_id.h"

namespace enterprise_management {
class DeviceManagementResponse;
}

namespace signin {
class AccessTokenFetcher;
class IdentityManager;
struct AccessTokenInfo;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}

class GoogleServiceAuthError;

namespace policy {

// Interacts with the device management service and determines whether Android
// management is enabled for the user or not. Uses the IdentityManager to
// acquire access tokens for the device management.
class AndroidManagementClient {
 public:
  // Indicates result of the android management check.
  enum class Result {
    MANAGED,    // Android management is enabled.
    UNMANAGED,  // Android management is disabled.
    ERROR,      // Received a error.
  };

  // A callback which receives Result status of an operation.
  using StatusCallback = base::OnceCallback<void(Result)>;

  AndroidManagementClient(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const CoreAccountId& account_id,
      signin::IdentityManager* identity_manager);
  ~AndroidManagementClient();

  // Starts sending of check Android management request to DM server, issues
  // access token if necessary. |callback| is called on check Android
  // management completion.
  void StartCheckAndroidManagement(StatusCallback callback);

  // |access_token| is owned by caller and must exist before
  // StartCheckAndroidManagement is called for testing.
  static void SetAccessTokenForTesting(const char* access_token);

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // Requests an access token.
  void RequestAccessToken();

  // Sends a CheckAndroidManagementRequest to DM server.
  void CheckAndroidManagement(const std::string& access_token);

  // Callback for check Android management requests.
  void OnAndroidManagementChecked(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Used to communicate with the device management service.
  DeviceManagementService* const device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<DeviceManagementService::Job> request_job_;

  // The account ID that will be used for the access token fetch.
  const CoreAccountId account_id_;

  signin::IdentityManager* identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  StatusCallback callback_;

  base::WeakPtrFactory<AndroidManagementClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AndroidManagementClient);
};

// Outputs the stringified |result| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os,
                         AndroidManagementClient::Result result);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ANDROID_MANAGEMENT_CLIENT_H_
