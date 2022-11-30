// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_device_source.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_handler_registry.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<VapidKeyManager> vapid_key_manager,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingMessageSender> message_sender,
    std::unique_ptr<SharingDeviceSource> device_source,
    std::unique_ptr<SharingHandlerRegistry> handler_registry,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    syncer::SyncService* sync_service)
    : sync_prefs_(std::move(sync_prefs)),
      vapid_key_manager_(std::move(vapid_key_manager)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      message_sender_(std::move(message_sender)),
      device_source_(std::move(device_source)),
      handler_registry_(std::move(handler_registry)),
      fcm_handler_(std::move(fcm_handler)),
      sync_service_(sync_service),
      backoff_entry_(&kRetryBackoffPolicy),
      state_(State::DISABLED) {
  // If device has already registered before, start listening to FCM right away
  // to avoid missing messages.
  if (sync_prefs_ && sync_prefs_->GetFCMRegistration())
    fcm_handler_->StartListening();

  if (sync_service_ && !sync_service_->HasObserver(this))
    sync_service_->AddObserver(this);

  // Only unregister if sync is disabled (not initializing).
  if (IsSyncDisabledForSharing(sync_service_)) {
    // state_ is kept as State::DISABLED as SharingService has never registered,
    // and only doing clean up via UnregisterDevice().
    UnregisterDevice();
  }
}

SharingService::~SharingService() {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
}

std::unique_ptr<syncer::DeviceInfo> SharingService::GetDeviceByGuid(
    const std::string& guid) const {
  return device_source_->GetDeviceByGuid(guid);
}

SharingService::SharingDeviceList SharingService::GetDeviceCandidates(
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  return device_source_->GetDeviceCandidates(required_feature);
}

base::OnceClosure SharingService::SendMessageToDevice(
    const syncer::DeviceInfo& device,
    base::TimeDelta response_timeout,
    chrome_browser_sharing::SharingMessage message,
    SharingMessageSender::ResponseCallback callback) {
  return message_sender_->SendMessageToDevice(
      device, response_timeout, std::move(message),
      SharingMessageSender::DelegateType::kFCM, std::move(callback));
}

void SharingService::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  handler_registry_->RegisterSharingHandler(std::move(handler), payload_case);
}

void SharingService::UnregisterSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  handler_registry_->UnregisterSharingHandler(payload_case);
}

void SharingService::SetNotificationActionHandler(
    const std::string& notification_id,
    NotificationActionCallback callback) {
  if (callback)
    notification_action_handlers_[notification_id] = callback;
  else
    notification_action_handlers_.erase(notification_id);
}

SharingService::NotificationActionCallback
SharingService::GetNotificationActionHandler(
    const std::string& notification_id) const {
  auto iter = notification_action_handlers_.find(notification_id);
  return iter == notification_action_handlers_.end()
             ? NotificationActionCallback()
             : iter->second;
}

SharingDeviceSource* SharingService::GetDeviceSource() const {
  return device_source_.get();
}

SharingService::State SharingService::GetStateForTesting() const {
  return state_;
}

SharingSyncPreference* SharingService::GetSyncPreferencesForTesting() const {
  return sync_prefs_.get();
}

SharingFCMHandler* SharingService::GetFCMHandlerForTesting() const {
  return fcm_handler_.get();
}

SharingMessageSender* SharingService::GetMessageSenderForTesting() const {
  return message_sender_.get();
}

SharingMessageHandler* SharingService::GetSharingHandlerForTesting(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) const {
  return handler_registry_->GetSharingHandler(payload_case);
}

void SharingService::OnSyncShutdown(syncer::SyncService* sync) {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SharingService::OnStateChanged(syncer::SyncService* sync) {
  if (IsSyncEnabledForSharing(sync_service_) && state_ == State::DISABLED) {
    state_ = State::REGISTERING;
    RegisterDevice();
  } else if (IsSyncDisabledForSharing(sync_service_) &&
             state_ == State::ACTIVE) {
    state_ = State::UNREGISTERING;
    fcm_handler_->StopListening();
    sync_prefs_->ClearVapidKeyChangeObserver();
    UnregisterDevice();
  }
}

void SharingService::RefreshVapidKey() {
  if (vapid_key_manager_->RefreshCachedKey())
    RegisterDevice();
}

void SharingService::RegisterDevice() {
  sharing_device_registration_->RegisterDevice(base::BindOnce(
      &SharingService::OnDeviceRegistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::RegisterDeviceInTesting(
    std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
    SharingDeviceRegistration::RegistrationCallback callback) {
  sharing_device_registration_->SetEnabledFeaturesForTesting(
      std::move(enabled_features));
  sharing_device_registration_->RegisterDevice(std::move(callback));
}

void SharingService::UnregisterDevice() {
  sharing_device_registration_->UnregisterDevice(base::BindOnce(
      &SharingService::OnDeviceUnregistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::OnDeviceRegistered(
    SharingDeviceRegistrationResult result) {
  LogSharingRegistrationResult(result);
  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      backoff_entry_.InformOfRequest(true);
      if (state_ == State::REGISTERING) {
        if (IsSyncEnabledForSharing(sync_service_)) {
          state_ = State::ACTIVE;
          fcm_handler_->StartListening();
          // Listen for further VAPID key changes for re-registration.
          // state_ is kept as State::ACTIVE during re-registration.
          sync_prefs_->SetVapidKeyChangeObserver(
              base::BindRepeating(&SharingService::RefreshVapidKey,
                                  weak_ptr_factory_.GetWeakPtr()));
        } else if (IsSyncDisabledForSharing(sync_service_)) {
          // In case sync is disabled during registration, unregister it.
          state_ = State::UNREGISTERING;
          UnregisterDevice();
        }
      }
      // For registration as result of VAPID key change, state_ will be
      // State::ACTIVE, and we don't need to start listeners.
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      backoff_entry_.InformOfRequest(false);
      // Transient error - try again after a delay.
      LOG(ERROR) << "Device registration failed with transient error";
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostDelayedTask(FROM_HERE,
                            base::BindOnce(&SharingService::RegisterDevice,
                                           weak_ptr_factory_.GetWeakPtr()),
                            backoff_entry_.GetTimeUntilRelease());
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
    case SharingDeviceRegistrationResult::kInternalError:
      backoff_entry_.InformOfRequest(false);
      // No need to bother retrying in the case of one of fatal errors.
      LOG(ERROR) << "Device registration failed with fatal error";
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Register device cannot return kDeviceNotRegistered.
      NOTREACHED();
  }
}

void SharingService::OnDeviceUnregistered(
    SharingDeviceRegistrationResult result) {
  LogSharingUnregistrationResult(result);
  if (IsSyncEnabledForSharing(sync_service_)) {
    // In case sync is enabled during un-registration, register it.
    state_ = State::REGISTERING;
    RegisterDevice();
  } else {
    state_ = State::DISABLED;
  }

  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      // Successfully unregistered, no-op
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      LOG(ERROR) << "Device un-registration failed with transient error";
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
    case SharingDeviceRegistrationResult::kInternalError:
      LOG(ERROR) << "Device un-registration failed with fatal error";
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Device has not been registered, no-op.
      break;
  }
}
