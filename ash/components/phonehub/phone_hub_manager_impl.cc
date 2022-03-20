// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/phone_hub_manager_impl.h"

#include "ash/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "ash/components/phonehub/browser_tabs_model_controller.h"
#include "ash/components/phonehub/browser_tabs_model_provider.h"
#include "ash/components/phonehub/camera_roll_download_manager.h"
#include "ash/components/phonehub/camera_roll_manager_impl.h"
#include "ash/components/phonehub/connection_scheduler_impl.h"
#include "ash/components/phonehub/cros_state_sender.h"
#include "ash/components/phonehub/do_not_disturb_controller_impl.h"
#include "ash/components/phonehub/feature_status_provider_impl.h"
#include "ash/components/phonehub/find_my_device_controller_impl.h"
#include "ash/components/phonehub/invalid_connection_disconnector.h"
#include "ash/components/phonehub/message_receiver_impl.h"
#include "ash/components/phonehub/message_sender_impl.h"
#include "ash/components/phonehub/multidevice_feature_access_manager_impl.h"
#include "ash/components/phonehub/multidevice_setup_state_updater.h"
#include "ash/components/phonehub/mutable_phone_model.h"
#include "ash/components/phonehub/notification_interaction_handler_impl.h"
#include "ash/components/phonehub/notification_manager_impl.h"
#include "ash/components/phonehub/notification_processor.h"
#include "ash/components/phonehub/onboarding_ui_tracker_impl.h"
#include "ash/components/phonehub/phone_model.h"
#include "ash/components/phonehub/phone_status_processor.h"
#include "ash/components/phonehub/recent_apps_interaction_handler_impl.h"
#include "ash/components/phonehub/screen_lock_manager_impl.h"
#include "ash/components/phonehub/tether_controller_impl.h"
#include "ash/components/phonehub/user_action_recorder_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/services/secure_channel/public/cpp/client/connection_manager_impl.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {
namespace {
const char kSecureChannelFeatureName[] = "phone_hub";
const char kConnectionResultMetricName[] = "PhoneHub.Connection.Result";
const char kConnectionDurationMetricName[] = "PhoneHub.Connection.Duration";
const char kConnectionLatencyMetricName[] = "PhoneHub.Connectivity.Latency";
}  // namespace
namespace phonehub {

PhoneHubManagerImpl::PhoneHubManagerImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<BrowserTabsModelProvider> browser_tabs_model_provider,
    std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager,
    const base::RepeatingClosure& show_multidevice_setup_dialog_callback)
    : connection_manager_(
          std::make_unique<secure_channel::ConnectionManagerImpl>(
              multidevice_setup_client,
              device_sync_client,
              secure_channel_client,
              kSecureChannelFeatureName,
              kConnectionResultMetricName,
              kConnectionLatencyMetricName,
              kConnectionDurationMetricName)),
      feature_status_provider_(std::make_unique<FeatureStatusProviderImpl>(
          device_sync_client,
          multidevice_setup_client,
          connection_manager_.get(),
          session_manager::SessionManager::Get(),
          chromeos::PowerManagerClient::Get())),
      user_action_recorder_(std::make_unique<UserActionRecorderImpl>(
          feature_status_provider_.get())),
      message_receiver_(
          std::make_unique<MessageReceiverImpl>(connection_manager_.get())),
      message_sender_(
          std::make_unique<MessageSenderImpl>(connection_manager_.get())),
      phone_model_(std::make_unique<MutablePhoneModel>()),
      cros_state_sender_(
          std::make_unique<CrosStateSender>(message_sender_.get(),
                                            connection_manager_.get(),
                                            multidevice_setup_client,
                                            phone_model_.get())),
      do_not_disturb_controller_(std::make_unique<DoNotDisturbControllerImpl>(
          message_sender_.get(),
          user_action_recorder_.get())),
      connection_scheduler_(std::make_unique<ConnectionSchedulerImpl>(
          connection_manager_.get(),
          feature_status_provider_.get())),
      find_my_device_controller_(std::make_unique<FindMyDeviceControllerImpl>(
          message_sender_.get(),
          user_action_recorder_.get())),
      multidevice_feature_access_manager_(
          std::make_unique<MultideviceFeatureAccessManagerImpl>(
              pref_service,
              multidevice_setup_client,
              feature_status_provider_.get(),
              message_sender_.get(),
              connection_scheduler_.get())),
      screen_lock_manager_(
          features::IsEcheSWAEnabled()
              ? std::make_unique<ScreenLockManagerImpl>(pref_service)
              : nullptr),
      notification_interaction_handler_(
          features::IsEcheSWAEnabled()
              ? std::make_unique<NotificationInteractionHandlerImpl>()
              : nullptr),
      notification_manager_(
          std::make_unique<NotificationManagerImpl>(message_sender_.get(),
                                                    user_action_recorder_.get(),
                                                    multidevice_setup_client)),
      onboarding_ui_tracker_(std::make_unique<OnboardingUiTrackerImpl>(
          pref_service,
          feature_status_provider_.get(),
          multidevice_setup_client,
          show_multidevice_setup_dialog_callback)),
      notification_processor_(
          std::make_unique<NotificationProcessor>(notification_manager_.get())),
      recent_apps_interaction_handler_(
          features::IsEcheSWAEnabled()
              ? std::make_unique<RecentAppsInteractionHandlerImpl>(
                    pref_service,
                    multidevice_setup_client,
                    multidevice_feature_access_manager_.get())
              : nullptr),
      phone_status_processor_(std::make_unique<PhoneStatusProcessor>(
          do_not_disturb_controller_.get(),
          feature_status_provider_.get(),
          message_receiver_.get(),
          find_my_device_controller_.get(),
          multidevice_feature_access_manager_.get(),
          screen_lock_manager_.get(),
          notification_processor_.get(),
          multidevice_setup_client,
          phone_model_.get(),
          recent_apps_interaction_handler_.get())),
      tether_controller_(
          std::make_unique<TetherControllerImpl>(phone_model_.get(),
                                                 user_action_recorder_.get(),
                                                 multidevice_setup_client)),
      browser_tabs_model_provider_(std::move(browser_tabs_model_provider)),
      browser_tabs_model_controller_(
          std::make_unique<BrowserTabsModelController>(
              multidevice_setup_client,
              browser_tabs_model_provider_.get(),
              phone_model_.get())),
      multidevice_setup_state_updater_(
          std::make_unique<MultideviceSetupStateUpdater>(
              pref_service,
              multidevice_setup_client,
              multidevice_feature_access_manager_.get())),
      invalid_connection_disconnector_(
          std::make_unique<InvalidConnectionDisconnector>(
              connection_manager_.get(),
              phone_model_.get())),
      camera_roll_manager_(features::IsPhoneHubCameraRollEnabled()
                               ? std::make_unique<CameraRollManagerImpl>(
                                     message_receiver_.get(),
                                     message_sender_.get(),
                                     multidevice_setup_client,
                                     connection_manager_.get(),
                                     std::move(camera_roll_download_manager))
                               : nullptr) {}

PhoneHubManagerImpl::~PhoneHubManagerImpl() = default;

BrowserTabsModelProvider* PhoneHubManagerImpl::GetBrowserTabsModelProvider() {
  return browser_tabs_model_provider_.get();
}

CameraRollManager* PhoneHubManagerImpl::GetCameraRollManager() {
  return camera_roll_manager_.get();
}

ConnectionScheduler* PhoneHubManagerImpl::GetConnectionScheduler() {
  return connection_scheduler_.get();
}

DoNotDisturbController* PhoneHubManagerImpl::GetDoNotDisturbController() {
  return do_not_disturb_controller_.get();
}

FeatureStatusProvider* PhoneHubManagerImpl::GetFeatureStatusProvider() {
  return feature_status_provider_.get();
}

FindMyDeviceController* PhoneHubManagerImpl::GetFindMyDeviceController() {
  return find_my_device_controller_.get();
}

MultideviceFeatureAccessManager*
PhoneHubManagerImpl::GetMultideviceFeatureAccessManager() {
  return multidevice_feature_access_manager_.get();
}

NotificationInteractionHandler*
PhoneHubManagerImpl::GetNotificationInteractionHandler() {
  return notification_interaction_handler_.get();
}

NotificationManager* PhoneHubManagerImpl::GetNotificationManager() {
  return notification_manager_.get();
}

OnboardingUiTracker* PhoneHubManagerImpl::GetOnboardingUiTracker() {
  return onboarding_ui_tracker_.get();
}

PhoneModel* PhoneHubManagerImpl::GetPhoneModel() {
  return phone_model_.get();
}

RecentAppsInteractionHandler*
PhoneHubManagerImpl::GetRecentAppsInteractionHandler() {
  return recent_apps_interaction_handler_.get();
}

ScreenLockManager* PhoneHubManagerImpl::GetScreenLockManager() {
  return screen_lock_manager_.get();
}

TetherController* PhoneHubManagerImpl::GetTetherController() {
  return tether_controller_.get();
}

UserActionRecorder* PhoneHubManagerImpl::GetUserActionRecorder() {
  return user_action_recorder_.get();
}

// NOTE: These should be destroyed in the opposite order of how these objects
// are initialized in the constructor.
void PhoneHubManagerImpl::Shutdown() {
  camera_roll_manager_.reset();
  invalid_connection_disconnector_.reset();
  multidevice_setup_state_updater_.reset();
  browser_tabs_model_controller_.reset();
  browser_tabs_model_provider_.reset();
  tether_controller_.reset();
  phone_status_processor_.reset();
  recent_apps_interaction_handler_.reset();
  notification_processor_.reset();
  onboarding_ui_tracker_.reset();
  notification_manager_.reset();
  notification_interaction_handler_.reset();
  screen_lock_manager_.reset();
  multidevice_feature_access_manager_.reset();
  find_my_device_controller_.reset();
  connection_scheduler_.reset();
  do_not_disturb_controller_.reset();
  cros_state_sender_.reset();
  phone_model_.reset();
  message_sender_.reset();
  message_receiver_.reset();
  user_action_recorder_.reset();
  feature_status_provider_.reset();
  connection_manager_.reset();
}

}  // namespace phonehub
}  // namespace ash
