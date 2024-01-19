// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_DESKTOP_API_CALL_FLOW_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_DESKTOP_API_CALL_FLOW_H_

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace push_notification {

// |PushNotificationDesktopApiCallFlow| is owned by a
// |PushNotificationServerClient| which shall only process a single request at a
// time. Therefore a new instance is require for each request. DO NOT REUSE.
class PushNotificationDesktopApiCallFlow {
 public:
  enum class PushNotificationApiCallFlowError {
    // Request could not be completed because the device is offline or has
    // issues
    // sending the HTTP request.
    kOffline,

    // Server endpoint could not be found.
    kEndpointNotFound,

    // Authentication error contacting back-end.
    kAuthenticationError,

    // Request was invalid.
    kBadRequest,

    // The server responded, but the response was not formatted correctly.
    kResponseMalformed,

    // Internal server error.
    kInternalServerError,

    // Unknown result.
    kUnknown
  };

  PushNotificationApiCallFlowError GetErrorForHttpResponseCode(
      int response_code);

  using ResultCallback =
      base::OnceCallback<void(const std::string& serialized_response)>;
  using ErrorCallback =
      base::OnceCallback<void(PushNotificationApiCallFlowError error)>;
  using QueryParameters = std::vector<std::pair<std::string, std::string>>;

  PushNotificationDesktopApiCallFlow() = default;
  virtual ~PushNotificationDesktopApiCallFlow() = default;

  // Starts the API POST request call.
  //   |request_url|: The URL endpoint of the API request.
  //   |serialized_request|: A serialized proto containing the request data.
  //   |access_token|: The access token for whom to make the request.
  //   |result_callback|: Called when the flow completes successfully
  //                      with a serialized response proto.
  //   |error_callback|: Called when the flow completes with an error.
  virtual void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) = 0;

  // Starts the API PATCH request call.
  //   |request_url|: The URL endpoint of the API request.
  //   |serialized_request|: A serialized proto containing the request data.
  //   |access_token|: The access token for whom to make the request.
  //   |result_callback|: Called when the flow completes successfully
  //                      with a serialized response proto.
  //   |error_callback|: Called when the flow completes with an error.
  virtual void StartPatchRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) = 0;

  // Starts the API GET request call.
  //   |request_url|: The URL endpoint of the API request.
  //   |request_as_query_parameters|: The request proto represented as key-value
  //                                  pairs to be sent as query parameters.
  //                                  Note: A key can have multiple values.
  //   |access_token|: The access token for whom to make the request.
  //   |result_callback|: Called when the flow completes successfully
  //                      with a serialized response proto.
  //   |error_callback|: Called when the flow completes with an error.
  virtual void StartGetRequest(
      const GURL& request_url,
      const QueryParameters& request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) = 0;

  virtual void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation) = 0;
};

std::ostream& operator<<(
    std::ostream& stream,
    const PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError&
        error);

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_DESKTOP_API_CALL_FLOW_H_
