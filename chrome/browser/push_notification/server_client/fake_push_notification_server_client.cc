// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/server_client/fake_push_notification_server_client.h"

namespace push_notification {

FakePushNotificationServerClient::Factory::Factory() = default;

FakePushNotificationServerClient::Factory::~Factory() = default;

std::unique_ptr<PushNotificationServerClient>
FakePushNotificationServerClient::Factory::CreateInstance(
    std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  auto instance = std::make_unique<FakePushNotificationServerClient>();
  last_created_fake_server_client_ = instance.get();
  return instance;
}

FakePushNotificationServerClient::FakePushNotificationServerClient() = default;

FakePushNotificationServerClient::~FakePushNotificationServerClient() = default;

void FakePushNotificationServerClient::SetAccessTokenUsed(
    const std::string& token) {
  access_token_used_ = token;
}

void FakePushNotificationServerClient::
    InvokeRegisterWithPushNotificationServiceSuccessCallback(
        const proto::NotificationsMultiLoginUpdateResponse& response) {
  CHECK(register_with_push_notification_service_callback_);
  std::move(register_with_push_notification_service_callback_).Run(response);
}

void FakePushNotificationServerClient::
    InvokeRegisterWithPushNotificationServiceErrorCallback(
        PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
            error) {
  CHECK(register_with_push_notification_service_error_callback_);
  std::move(register_with_push_notification_service_error_callback_).Run(error);
}

void FakePushNotificationServerClient::RegisterWithPushNotificationService(
    const proto::NotificationsMultiLoginUpdateRequest& request,
    RegisterWithPushNotificationServiceCallback&& callback,
    ErrorCallback&& error_callback) {
  request_ = request;
  register_with_push_notification_service_callback_ = std::move(callback);
  register_with_push_notification_service_error_callback_ =
      std::move(error_callback);
}

const proto::NotificationsMultiLoginUpdateRequest&
FakePushNotificationServerClient::GetRequestProto() {
  return request_;
}

std::optional<std::string>
FakePushNotificationServerClient::GetAccessTokenUsed() {
  return access_token_used_;
}

}  // namespace push_notification
