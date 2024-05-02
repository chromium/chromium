// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"

#include "base/check.h"
#include "components/prefs/pref_service.h"

namespace push_notification {

PushNotificationServiceDesktopImpl::PushNotificationServiceDesktopImpl(
    PrefService* pref_service,
    instance_id::InstanceIDDriver* instance_id_driver,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      instance_id_driver_(instance_id_driver),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {
  CHECK(pref_service_);
  CHECK(instance_id_driver_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
}

PushNotificationServiceDesktopImpl::~PushNotificationServiceDesktopImpl() =
    default;

void PushNotificationServiceDesktopImpl::ShutdownHandler() {
  // Shutdown() should come before and it removes us from the list of app
  // handlers of gcm::GCMDriver so this shouldn't ever been called.
  NOTREACHED() << "The Push Notification Service should have removed itself "
                  "from the list of app handlers before this could be called.";
}

void PushNotificationServiceDesktopImpl::OnStoreReset() {
  // TODO(b/337874846): Clear prefs here.
}

void PushNotificationServiceDesktopImpl::OnMessage(
    const std::string& app_id,
    const gcm::IncomingMessage& message) {
  // TODO(b/321305014): Handle message here.
}

void PushNotificationServiceDesktopImpl::OnMessagesDeleted(
    const std::string& app_id) {}
void PushNotificationServiceDesktopImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED()
      << "The Push Notification Service shouldn't have sent messages upstream";
}
void PushNotificationServiceDesktopImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED()
      << "The Push Notification Service shouldn't have sent messages upstream";
}

// Intentional no-op. We don't support encryption/decryption of messages.
void PushNotificationServiceDesktopImpl::OnMessageDecryptionFailed(
    const std::string& app_id,
    const std::string& message_id,
    const std::string& error_message) {}

// PushNotificationService does not support messages from any other app.
bool PushNotificationServiceDesktopImpl::CanHandle(
    const std::string& app_id) const {
  return false;
}

void PushNotificationServiceDesktopImpl::Shutdown() {
  // TODO(b/306398998): Once fetching GCM token is implemented, reset the token
  // here.
  client_manager_.reset();
}

}  // namespace push_notification
