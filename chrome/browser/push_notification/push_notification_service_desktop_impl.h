// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_DESKTOP_IMPL_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_DESKTOP_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/push_notification/push_notification_service.h"

class PrefService;

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace push_notification {

// Desktop implementation of PushNotificationService.
class PushNotificationServiceDesktopImpl : public PushNotificationService,
                                           public KeyedService,
                                           public gcm::GCMAppHandler {
 public:
  explicit PushNotificationServiceDesktopImpl(
      PrefService* pref_service,
      instance_id::InstanceIDDriver* instance_id_driver,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  PushNotificationServiceDesktopImpl(
      const PushNotificationServiceDesktopImpl&) = delete;
  PushNotificationServiceDesktopImpl& operator=(
      const PushNotificationServiceDesktopImpl&) = delete;
  ~PushNotificationServiceDesktopImpl() override;

  // gcm::GCMAppHandler:
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  void OnMessageDecryptionFailed(const std::string& app_id,
                                 const std::string& message_id,
                                 const std::string& error_message) override;
  bool CanHandle(const std::string& app_id) const override;

 private:
  // KeyedService:
  void Shutdown() override;

  raw_ptr<const PrefService> pref_service_;
  raw_ptr<gcm::GCMDriver> gcm_driver_;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<PushNotificationServiceDesktopImpl> weak_ptr_factory_{
      this};
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_DESKTOP_IMPL_H_
