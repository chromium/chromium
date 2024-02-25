// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_CLIENT_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GoogleServiceAuthError;
class NearbyShareHttpNotifier;

// An implementation of NearbyShareClient that fetches access tokens for the
// primary account and makes HTTP calls using ash::nearby::NearbyApiCallFlow.
// Callbacks are guaranteed to not be invoked after NearbyShareClientImpl is
// destroyed.
class NearbyShareClientImpl : public NearbyShareClient {
 public:
  // Creates the client using |url_loader_factory| to make the HTTP request
  // through |api_call_flow|.
  NearbyShareClientImpl(
      std::unique_ptr<ash::nearby::NearbyApiCallFlow> api_call_flow,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      NearbyShareHttpNotifier* notifier);
  ~NearbyShareClientImpl() override;

  NearbyShareClientImpl(NearbyShareClientImpl&) = delete;
  NearbyShareClientImpl& operator=(NearbyShareClientImpl&) = delete;

  // NearbyShareClient:
  void UpdateDevice(const nearby::sharing::proto::UpdateDeviceRequest& request,
                    UpdateDeviceCallback&& callback,
                    ErrorCallback&& error_callback) override;
  void ListContactPeople(
      const nearby::sharing::proto::ListContactPeopleRequest& request,
      ListContactPeopleCallback&& callback,
      ErrorCallback&& error_callback) override;
  void ListPublicCertificates(
      const nearby::sharing::proto::ListPublicCertificatesRequest& request,
      ListPublicCertificatesCallback&& callback,
      ErrorCallback&& error_callback) override;
  std::string GetAccessTokenUsed() override;

 private:
  enum class RequestType { kGet, kPost, kPatch };

  // Starts a call to the API given by |request_url|. The client first fetches
  // the access token and then makes the HTTP request.
  //   |request_url|: API endpoint.
  //   |request_type|: Whether the request is a GET, POST, or PATCH.
  //   |serialized_request|: Serialized request message proto that will be sent
  //                         as the body of a POST or PATCH request. Null if
  //                         request type is not POST or PATCH.
  //   |request_as_query_parameters|: The request message proto represented as
  //                                  key-value pairs that will be sent as query
  //                                  parameters in a GET request. Note: A key
  //                                  can have multiple values. Null if request
  //                                  type is not GET.
  //   |response_callback|: Callback for a successful request.
  //   |error_callback|: Callback for a failed request.
  //   |partial_traffic_annotation|: A partial tag used to mark a source of
  //                                 network traffic.
  template <class ResponseProto>
  void MakeApiCall(
      const GURL& request_url,
      RequestType request_type,
      const std::optional<std::string>& serialized_request,
      const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
          request_as_query_parameters,
      base::OnceCallback<void(const ResponseProto&)>&& response_callback,
      ErrorCallback&& error_callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Called when the access token is obtained so the API request can be made.
  template <class ResponseProto>
  void OnAccessTokenFetched(
      RequestType request_type,
      const std::optional<std::string>& serialized_request,
      const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
          request_as_query_parameters,
      base::OnceCallback<void(const ResponseProto&)>&& response_callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // Called when ash::nearby::NearbyApiCallFlow completes successfully to
  // deserialize and return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      base::OnceCallback<void(const ResponseProto&)>&& result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(ash::nearby::NearbyHttpError error);

  // Constructs and executes the actual HTTP request.
  std::unique_ptr<ash::nearby::NearbyApiCallFlow> api_call_flow_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  // Fetches the access token authorizing the API calls.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<NearbyShareHttpNotifier> notifier_ = nullptr;

  // True if an API call has been started. Remains true even after the API call
  // completes.
  bool has_call_started_;

  // URL of the current request.
  GURL request_url_;

  // The access token fetched by |access_token_fetcher_|.
  std::string access_token_used_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  base::WeakPtrFactory<NearbyShareClientImpl> weak_ptr_factory_{this};
};

// Implementation of NearbyShareClientFactory.
class NearbyShareClientFactoryImpl : public NearbyShareClientFactory {
 public:
  // |identity_manager|: Gets the user's access token.
  //     Not owned, so |identity_manager| needs to outlive this object.
  // |url_loader_factory|: Used to make the HTTP requests.
  NearbyShareClientFactoryImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      NearbyShareHttpNotifier* notifier);
  ~NearbyShareClientFactoryImpl() override;

  NearbyShareClientFactoryImpl(NearbyShareClientFactoryImpl&) = delete;
  NearbyShareClientFactoryImpl& operator=(NearbyShareClientFactoryImpl&) =
      delete;

  // NearbyShareClientFactory:
  std::unique_ptr<NearbyShareClient> CreateInstance() override;

 private:
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<NearbyShareHttpNotifier> notifier_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_CLIENT_IMPL_H_
