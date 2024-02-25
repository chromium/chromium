// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_ACCOUNT_INITIALIZER_H_
#define CHROME_BROWSER_POLICY_DEVICE_ACCOUNT_INITIALIZER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace policy {

// Implements the logic that initializes device account during enrollment.
//   1. Download the OAuth2 authorization code for device-level API access.
//   2. Download the OAuth2 refresh token for device-level API access and store
//      it.
//   3. Store API refresh token.
// This class does not handle OnClientError in CloudPolicyClient::Observer.
// Instance owner, that also owns CloudPolicyClient should handle those errors.
class DeviceAccountInitializer : public CloudPolicyClient::Observer,
                                 public gaia::GaiaOAuthClient::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when OAuth2 refresh token fetching is complete. In test
    // environment authorization code might be empty, this would be communicated
    // by |empty_token|.
    virtual void OnDeviceAccountTokenFetched(bool empty_token) = 0;

    // Called when OAuth2 refresh token is successfully stored.
    virtual void OnDeviceAccountTokenStored() = 0;

    // Called when an error happens during token fetching. `dm_status` is
    // nullopt if error happened before requesting device management service.
    virtual void OnDeviceAccountTokenFetchError(
        std::optional<DeviceManagementStatus> dm_status) = 0;

    // Called when an error happens during token saving.
    virtual void OnDeviceAccountTokenStoreError() = 0;

    // Called when an error happens during cloud policy client calls.
    virtual void OnDeviceAccountClientError(DeviceManagementStatus status) = 0;

    // Returns the device type that should be sent to the device management
    // server when requesting auth codes.
    virtual enterprise_management::DeviceServiceApiAccessRequest::DeviceType
    GetRobotAuthCodeDeviceType() = 0;

    // Returns the oauth scopes for which to request auth codes.
    virtual std::set<std::string> GetRobotOAuthScopes() = 0;

    // Returns a url loader factory that the DeviceAccountInitializer will use
    // for GAIA requests.
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;
  };

  DeviceAccountInitializer(CloudPolicyClient* client, Delegate* delegate);
  DeviceAccountInitializer(const DeviceAccountInitializer&) = delete;
  DeviceAccountInitializer& operator=(const DeviceAccountInitializer&) = delete;
  ~DeviceAccountInitializer() override;

  // Starts process that downloads OAuth2 auth code and exchanges it to OAuth2
  // refresh token. Either completion or error notification would be called on
  // the consumer.
  void FetchToken();

  // Stores OAuth2 refresh token. Either completion or error notification would
  // be called on the consumer.
  void StoreToken();

  // Cancels all ongoing processes, nothing will be called on consumer.
  void Stop();

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // GaiaOAuthClient::Delegate:
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  // Handles completion of the robot token store operation.
  void HandleStoreRobotAuthTokenResult(bool result);

  // Handles the fetching auth codes for robot accounts during enrollment.
  void OnRobotAuthCodesFetched(DeviceManagementStatus status,
                               const std::string& auth_code);

  // Owned by this class owner.
  raw_ptr<CloudPolicyClient> client_;
  raw_ptr<Delegate> delegate_;

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  // Flag that undicates if there are requests that were not completed yet.
  // It is used to ignore CloudPolicyClient errors that are not relevant to
  // this class.
  bool handling_request_;

  // The robot account refresh token.
  std::string robot_refresh_token_;

  base::WeakPtrFactory<DeviceAccountInitializer> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_ACCOUNT_INITIALIZER_H_
