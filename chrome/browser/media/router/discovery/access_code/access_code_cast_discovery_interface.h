// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_DISCOVERY_INTERFACE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_DISCOVERY_INTERFACE_H_

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace media_router {

// AccessCodeCastDiscoveryInterface is responsible with communicating with
// the casting discovery server.
class AccessCodeCastDiscoveryInterface {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  using DiscoveryDeviceCallback =
      base::OnceCallback<void(std::optional<DiscoveryDevice>,
                              access_code_cast::mojom::AddSinkResultCode)>;

  using AddSinkResultCode = access_code_cast::mojom::AddSinkResultCode;

  AccessCodeCastDiscoveryInterface(Profile* profile,
                                   const std::string& access_code,
                                   LoggerImpl* logger,
                                   signin::IdentityManager* identity_manager);

  virtual ~AccessCodeCastDiscoveryInterface();

  AccessCodeCastDiscoveryInterface(
      const AccessCodeCastDiscoveryInterface& other) = delete;
  AccessCodeCastDiscoveryInterface& operator=(
      const AccessCodeCastDiscoveryInterface& other) = delete;

  // ValidateDiscoveryAccessCode is an asynchronous call that attempts to
  // validate given |access_code| with the discovery server. The status
  // of this attempt will be stored in the |callback| -- either returning an
  // error or the actual DiscoveryDevice found on the discovery server.
  // |std::optional<DiscoveryDevice>| will always have a value if an
  // AddSinkResultCode::OK is returned.
  void ValidateDiscoveryAccessCode(DiscoveryDeviceCallback callback);

  // Testing methods, do not use these outside of tests.
  void SetCallbackForTesting(DiscoveryDeviceCallback callback) {
    callback_ = std::move(callback);
  }

  void SetEndpointFetcherForTesting(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
    endpoint_fetcher_ = std::move(endpoint_fetcher);
  }

  std::unique_ptr<EndpointFetcher> CreateEndpointFetcherForTesting(
      const std::string& access_code);

  void HandleServerErrorForTesting(
      std::unique_ptr<EndpointResponse> endpoint_response);

 private:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const std::string& access_code);

  void SetDeviceCapabilitiesField(
      chrome_browser_media::proto::DeviceCapabilities* device_proto,
      bool value,
      const std::string& key);
  void SetNetworkInfoField(
      chrome_browser_media::proto::NetworkInfo* network_proto,
      const std::string& value,
      const std::string& key);
  std::pair<std::optional<DiscoveryDevice>, AddSinkResultCode>
  ConstructDiscoveryDeviceFromJson(base::Value json_response);
  void HandleDiscoveryDeviceJsonError(const std::string& field_missing);
  void HandleServerResponse(std::unique_ptr<EndpointResponse> response);

  // Should only be called if the response has a error_type set in the struct.
  void HandleServerError(std::unique_ptr<EndpointResponse> response);

  // Function that runs the member variable callback with the given error.
  void ReportErrorViaCallback(AddSinkResultCode error);

  AddSinkResultCode GetErrorFromResponse(const base::Value& response);
  AddSinkResultCode IsResponseValid(const std::optional<base::Value>& response);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  // Access code passed down from the WebUI and used in the construction of the
  // discovery interface object.
  const std::string access_code_;

  const raw_ptr<LoggerImpl, DanglingUntriaged> logger_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  std::unique_ptr<EndpointFetcher> endpoint_fetcher_;

  DiscoveryDeviceCallback callback_;

  base::WeakPtrFactory<AccessCodeCastDiscoveryInterface> weak_ptr_factory_{
      this};
};
}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_DISCOVERY_INTERFACE_H_
