// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_FAKE_PUSH_NOTIFICATION_SERVER_CLIENT_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_FAKE_PUSH_NOTIFICATION_SERVER_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/push_notification/protos/notifications_multi_login_update.pb.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client_desktop_impl.h"

namespace push_notification {

// A fake implementation of the Push Notification Desktop HTTP client that
// stores all request data. Only use in unit tests.
class FakePushNotificationServerClient : public PushNotificationServerClient {
 public:
  // Factory that creates `FakePushNotificationServerClient` instances. Use
  // in PushNotificationServerClientDesktopImpl::Factory::SetFactoryForTesting()
  // in unit tests.
  class Factory : public PushNotificationServerClientDesktopImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    FakePushNotificationServerClient* fake_server_client() {
      return last_created_fake_server_client_;
    }

   private:
    // PushNotificationServerClientDesktopImpl::Factory:
    std::unique_ptr<PushNotificationServerClient> CreateInstance(
        std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
        override;

    // Dangling pointer detection is disabled because this is only used in
    // testing and is necessary to allow tests to invoke
    // FakePushNotificationServerClient methods.
    raw_ptr<FakePushNotificationServerClient, DisableDanglingPtrDetection>
        last_created_fake_server_client_;
  };

  FakePushNotificationServerClient();
  ~FakePushNotificationServerClient() override;

  void SetAccessTokenUsed(const std::string& token);

  void InvokeRegisterWithPushNotificationServiceSuccessCallback(
      const proto::NotificationsMultiLoginUpdateResponse& response);
  void InvokeRegisterWithPushNotificationServiceErrorCallback(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
          error);

  const proto::NotificationsMultiLoginUpdateRequest& GetRequestProto();

  bool HasRegisterWithPushNotificationServiceCallback() {
    return !register_with_push_notification_service_callback_.is_null();
  }
  bool HasErrorCallback() {
    return !register_with_push_notification_service_error_callback_.is_null();
  }

 private:
  // PushNotificationServerClient:
  void RegisterWithPushNotificationService(
      const proto::NotificationsMultiLoginUpdateRequest& request,
      RegisterWithPushNotificationServiceCallback&& callback,
      ErrorCallback&& error_callback) override;
  std::optional<std::string> GetAccessTokenUsed() override;

  std::string access_token_used_;
  proto::NotificationsMultiLoginUpdateRequest request_;
  RegisterWithPushNotificationServiceCallback
      register_with_push_notification_service_callback_;
  ErrorCallback register_with_push_notification_service_error_callback_;
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_SERVER_CLIENT_FAKE_PUSH_NOTIFICATION_SERVER_CLIENT_H_
