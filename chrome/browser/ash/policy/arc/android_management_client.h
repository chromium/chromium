// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ARC_ANDROID_MANAGEMENT_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ARC_ANDROID_MANAGEMENT_CLIENT_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "google_apis/gaia/core_account_id.h"

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

struct DMServerJobResult;

// AndroidManagementClient is an interface to check the Android management
// status.
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

  virtual ~AndroidManagementClient() = default;

  // Starts sending of check Android management request to DM server, issues
  // access token if necessary. |callback| is called on check Android
  // management completion.
  virtual void StartCheckAndroidManagement(StatusCallback callback) = 0;
};

// Interacts with the device management service and determines whether Android
// management is enabled for the user or not. Uses the IdentityManager to
// acquire access tokens for the device management.
class AndroidManagementClientImpl : public AndroidManagementClient {
 public:
  AndroidManagementClientImpl(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const CoreAccountId& account_id,
      signin::IdentityManager* identity_manager);

  AndroidManagementClientImpl(const AndroidManagementClientImpl&) = delete;
  AndroidManagementClientImpl& operator=(const AndroidManagementClientImpl&) =
      delete;

  ~AndroidManagementClientImpl() override;

  // AndroidManagementClient override:
  void StartCheckAndroidManagement(StatusCallback callback) override;

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
  void OnAndroidManagementChecked(DMServerJobResult result);

  // Used to communicate with the device management service.
  const raw_ptr<DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<DeviceManagementService::Job> request_job_;

  // The account ID that will be used for the access token fetch.
  const CoreAccountId account_id_;

  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  StatusCallback callback_;

  base::WeakPtrFactory<AndroidManagementClientImpl> weak_ptr_factory_{this};
};

// Outputs the stringified |result| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os,
                         AndroidManagementClient::Result result);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ARC_ANDROID_MANAGEMENT_CLIENT_H_
