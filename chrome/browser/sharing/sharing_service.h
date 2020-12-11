// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "net/base/backoff_entry.h"

#if defined(OS_ANDROID)
#include "chrome/browser/sharing/sharing_service_proxy_android.h"
#endif  // defined(OS_ANDROID)

namespace syncer {
class DeviceInfo;
class SyncService;
}  // namespace syncer

class SharingFCMHandler;
class SharingHandlerRegistry;
class SharingMessageHandler;
class SharingSyncPreference;
class VapidKeyManager;
class SharingDeviceSource;
enum class SharingDeviceRegistrationResult;

// Class to manage lifecycle of sharing feature, and provide APIs to send
// sharing messages to other devices.
class SharingService : public KeyedService, public syncer::SyncServiceObserver {
 public:
  using SharingDeviceList = std::vector<std::unique_ptr<syncer::DeviceInfo>>;
  using NotificationActionCallback =
      base::RepeatingCallback<void(base::Optional<int> button, bool closed)>;

  enum class State {
    // Device is unregistered with FCM and Sharing is unavailable.
    DISABLED,
    // Device registration is in progress.
    REGISTERING,
    // Device is fully registered with FCM and Sharing is available.
    ACTIVE,
    // Device un-registration is in progress.
    UNREGISTERING
  };

  SharingService(
      std::unique_ptr<SharingSyncPreference> sync_prefs,
      std::unique_ptr<VapidKeyManager> vapid_key_manager,
      std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
      std::unique_ptr<SharingMessageSender> message_sender,
      std::unique_ptr<SharingDeviceSource> device_source,
      std::unique_ptr<SharingHandlerRegistry> handler_registry,
      std::unique_ptr<SharingFCMHandler> fcm_handler,
      syncer::SyncService* sync_service);
  SharingService(const SharingService&) = delete;
  SharingService& operator=(const SharingService&) = delete;
  ~SharingService() override;

  // Returns the device matching |guid|, or nullptr if no match was found.
  virtual std::unique_ptr<syncer::DeviceInfo> GetDeviceByGuid(
      const std::string& guid) const;

  // Returns a list of DeviceInfo that is available to receive messages.
  // All returned devices have the specified |required_feature|.
  virtual SharingDeviceList GetDeviceCandidates(
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const;

  // Sends a Sharing message to remote device.
  // |device|: The receiver device.
  // |response_timeout|: Maximum amount of time waiting for a response before
  // invoking |callback| with kAckTimeout.
  // |message|: Message to be sent.
  // |callback| will be invoked once a response has received from remote device,
  // or if operation has failed or timed out.
  virtual void SendMessageToDevice(
      const syncer::DeviceInfo& device,
      base::TimeDelta response_timeout,
      chrome_browser_sharing::SharingMessage message,
      SharingMessageSender::ResponseCallback callback);

  // Register SharingMessageHandler for |payload_cases|.
  void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

  // Unregister SharingMessageHandler for |payload_case|.
  void UnregisterSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

  // Sets a notification action handler for |notification_id|. Replaces any
  // previously set handlers for |notification_id|. |callback| may be a null
  // callback which clears the handler for |notification_id|.
  void SetNotificationActionHandler(const std::string& notification_id,
                                    NotificationActionCallback callback);

  // Returns the notification action handler for |notification_id| set by
  // SetNotificationActionHandler(). The returned callback may be null if no
  // handler has been set before for |notification_id|.
  NotificationActionCallback GetNotificationActionHandler(
      const std::string& notification_id) const;

  // Used to register devices with required capabilities in tests.
  void RegisterDeviceInTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
      SharingDeviceRegistration::RegistrationCallback callback);

  SharingDeviceSource* GetDeviceSource() const;

  // Returns the current state of SharingService for testing.
  State GetStateForTesting() const;

  // Returns SharingSyncPreference for integration tests.
  SharingSyncPreference* GetSyncPreferencesForTesting() const;

  // Returns SharingFCMHandler for testing.
  SharingFCMHandler* GetFCMHandlerForTesting() const;

  // Returns SharingMessageSender for testing.
  SharingMessageSender* GetMessageSenderForTesting() const;

 private:
  // Overrides for syncer::SyncServiceObserver.
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnStateChanged(syncer::SyncService* sync) override;

  void RefreshVapidKey();
  void RegisterDevice();
  void UnregisterDevice();

  void OnDeviceRegistered(SharingDeviceRegistrationResult result);
  void OnDeviceUnregistered(SharingDeviceRegistrationResult result);

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<VapidKeyManager> vapid_key_manager_;
  std::unique_ptr<SharingDeviceRegistration> sharing_device_registration_;
  std::unique_ptr<SharingMessageSender> message_sender_;
  std::unique_ptr<SharingDeviceSource> device_source_;
  std::unique_ptr<SharingHandlerRegistry> handler_registry_;
  std::unique_ptr<SharingFCMHandler> fcm_handler_;

  syncer::SyncService* sync_service_;

  net::BackoffEntry backoff_entry_;
  State state_;

#if defined(OS_ANDROID)
  SharingServiceProxyAndroid sharing_service_proxy_android_{this};
#endif  // defined(OS_ANDROID)

  // Map of notification id to notification handler callback.
  std::map<std::string, NotificationActionCallback>
      notification_action_handlers_;

  base::WeakPtrFactory<SharingService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
