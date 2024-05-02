// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_SERVER_CLIENT_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_SERVER_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow.h"

namespace push_notification::proto {
class NotificationsMultiLoginUpdateRequest;
class NotificationsMultiLoginUpdateResponse;
}  // namespace push_notification::proto

namespace push_notification {

// Interface for making API requests to the Push Notification service.
// Implementations shall only processes a single request, so create a new
// instance for each request you make. DO NOT REUSE.
class PushNotificationServerClient {
 public:
  using RegisterWithPushNotificationServiceCallback = base::OnceCallback<void(
      const proto::NotificationsMultiLoginUpdateResponse&)>;
  using ErrorCallback = base::OnceCallback<void(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError)>;

  PushNotificationServerClient() = default;
  virtual ~PushNotificationServerClient() = default;

  // Makes an POST request API call to register with the Push
  // Notification service.
  virtual void RegisterWithPushNotificationService(
      const proto::NotificationsMultiLoginUpdateRequest& request,
      RegisterWithPushNotificationServiceCallback&& callback,
      ErrorCallback&& error_callback) = 0;

  // Returns the access token used to make the request. If no request has been
  // made yet, this function will return std::nullopt.
  virtual std::optional<std::string> GetAccessTokenUsed() = 0;
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_PUSH_NOTIFICATION_SERVER_CLIENT_H_
