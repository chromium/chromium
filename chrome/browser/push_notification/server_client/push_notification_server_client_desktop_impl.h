// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_SERVER_CLIENT_DESKTOP_IMPL_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_SERVER_CLIENT_DESKTOP_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client.h"
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

namespace push_notification {

// An implementation of PushNotificationServerClient that fetches access
// tokens for the primary account and makes HTTP calls using
// PushNotificationDesktopApiCallFlow.
class PushNotificationServerClientDesktopImpl
    : public PushNotificationServerClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<PushNotificationServerClient> Create(
        std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PushNotificationServerClient> CreateInstance(
        std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

   private:
    static Factory* g_test_factory_;
  };

  ~PushNotificationServerClientDesktopImpl() override;

  PushNotificationServerClientDesktopImpl(
      PushNotificationServerClientDesktopImpl&) = delete;
  PushNotificationServerClientDesktopImpl& operator=(
      PushNotificationServerClientDesktopImpl&) = delete;

  // PushNotificationServerClient:
  void RegisterWithPushNotificationService(
      const proto::NotificationsMultiLoginUpdateRequest& request,
      RegisterWithPushNotificationServiceCallback&& callback,
      ErrorCallback&& error_callback) override;
  std::optional<std::string> GetAccessTokenUsed() override;

 private:
  // Creates the client using |url_loader_factory| to make the HTTP request
  // through |api_call_flow|.
  PushNotificationServerClientDesktopImpl(
      std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  enum class RequestType { kGet, kPost, kPatch };

  // Starts a call to the API given by |request_url|. The client first fetches
  // the access token and then makes the HTTP request.
  //   |request_url|: API endpoint.
  //   |request_type|: Whether the request is a GET, POST, or PATCH. (For MVP
  //                   only POST is used.
  //   |serialized_request|: Serialized request message proto that will be sent
  //                         as the body of a POST or PATCH request.
  //   |request_as_query_parameters|: Used for future GET requests but null if
  //                                  request type is not GET. Therefore always
  //                                  null for MVP.
  //   |response_callback|: Callback for a successful request.
  //   |error_callback|: Callback for a failed request.
  //   |partial_traffic_annotation|: A partial tag used to mark a source of
  //                                 network traffic.
  template <class ResponseProto>
  void MakeApiCall(
      const GURL& request_url,
      RequestType request_type,
      const std::optional<std::string>& serialized_request,
      const std::optional<PushNotificationDesktopApiCallFlow::QueryParameters>&
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
      const std::optional<PushNotificationDesktopApiCallFlow::QueryParameters>&
          request_as_query_parameters,
      base::OnceCallback<void(const ResponseProto&)>&& response_callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // Called when PushNotificationDesktopApiCallFlow completes successfully to
  // deserialize and return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      base::OnceCallback<void(const ResponseProto&)>&& result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
          error);

  // Constructs and executes the actual HTTP request.
  std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow_;

  // Fetches the access token authorizing the API calls.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // True if an API call has been started. Remains true even after the API call
  // completes.
  bool has_call_started_;

  // URL of the current request.
  GURL request_url_;

  // The access token fetched by |access_token_fetcher_|.
  std::optional<std::string> access_token_used_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::WeakPtrFactory<PushNotificationServerClientDesktopImpl>
      weak_ptr_factory_{this};
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_SERVER_CLIENT_DESKTOP_IMPL_H_
