// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_app_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/eche_app_ui/accessibility_provider.h"
#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "ash/webui/eche_app_ui/eche_connection_metrics_recorder.h"
#include "ash/webui/eche_app_ui/eche_connection_scheduler_impl.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/eche_connector_impl.h"
#include "ash/webui/eche_app_ui/eche_keyboard_layout_handler.h"
#include "ash/webui/eche_app_ui/eche_message_receiver_impl.h"
#include "ash/webui/eche_app_ui/eche_presence_manager.h"
#include "ash/webui/eche_app_ui/eche_signaler.h"
#include "ash/webui/eche_app_ui/eche_stream_orientation_observer.h"
#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "ash/webui/eche_app_ui/eche_tray_stream_status_observer.h"
#include "ash/webui/eche_app_ui/eche_uid_provider.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "ash/webui/eche_app_ui/system_info_provider.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager_impl.h"

namespace ash {
namespace {
const char kSecureChannelFeatureName[] = "eche";
}  // namespace

namespace eche_app {

EcheAppManager::EcheAppManager(
    PrefService* pref_service,
    std::unique_ptr<SystemInfo> system_info,
    phonehub::PhoneHubManager* phone_hub_manager,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<secure_channel::PresenceMonitorClient>
        presence_monitor_client,
    std::unique_ptr<AccessibilityProviderProxy> accessibility_provider_proxy,
    LaunchAppHelper::LaunchEcheAppFunction launch_eche_app_function,
    LaunchAppHelper::LaunchNotificationFunction launch_notification_function,
    LaunchAppHelper::CloseNotificationFunction close_notification_function)
    : phone_hub_manager_(phone_hub_manager),
      connection_manager_(
          std::make_unique<secure_channel::ConnectionManagerImpl>(
              multidevice_setup_client,
              device_sync_client,
              secure_channel_client,
              kSecureChannelFeatureName,
              std::make_unique<EcheConnectionMetricsRecorder>(),
              /*secure_channel_structured_metrics_logger*/ nullptr)),
      eche_connection_status_handler_(
          std::make_unique<EcheConnectionStatusHandler>()),
      feature_status_provider_(std::make_unique<EcheFeatureStatusProvider>(
          phone_hub_manager,
          multidevice_setup_client,
          connection_manager_.get(),
          eche_connection_status_handler_.get())),
      launch_app_helper_(
          std::make_unique<LaunchAppHelper>(phone_hub_manager,
                                            launch_eche_app_function,
                                            launch_notification_function,
                                            close_notification_function)),
      apps_launch_info_provider_(std::make_unique<AppsLaunchInfoProvider>(
          eche_connection_status_handler_.get())),
      stream_status_change_handler_(
          std::make_unique<EcheStreamStatusChangeHandler>(
              apps_launch_info_provider_.get(),
              eche_connection_status_handler_.get())),
      eche_notification_click_handler_(
          std::make_unique<EcheNotificationClickHandler>(
              phone_hub_manager,
              feature_status_provider_.get(),
              launch_app_helper_.get(),
              apps_launch_info_provider_.get())),
      connection_scheduler_(std::make_unique<EcheConnectionSchedulerImpl>(
          connection_manager_.get(),
          feature_status_provider_.get())),
      eche_connector_(
          std::make_unique<EcheConnectorImpl>(feature_status_provider_.get(),
                                              connection_manager_.get(),
                                              connection_scheduler_.get())),
      signaler_(std::make_unique<EcheSignaler>(
          eche_connector_.get(),
          connection_manager_.get(),
          apps_launch_info_provider_.get(),
          eche_connection_status_handler_.get())),
      message_receiver_(
          std::make_unique<EcheMessageReceiverImpl>(connection_manager_.get())),
      eche_presence_manager_(std::make_unique<EchePresenceManager>(
          feature_status_provider_.get(),
          device_sync_client,
          multidevice_setup_client,
          std::move(presence_monitor_client),
          eche_connector_.get(),
          message_receiver_.get())),
      uid_(std::make_unique<EcheUidProvider>(pref_service)),
      eche_recent_app_click_handler_(
          std::make_unique<EcheRecentAppClickHandler>(
              phone_hub_manager,
              feature_status_provider_.get(),
              launch_app_helper_.get(),
              stream_status_change_handler_.get(),
              apps_launch_info_provider_.get())),
      alert_generator_(
          std::make_unique<EcheAlertGenerator>(launch_app_helper_.get(),
                                               pref_service)),
      accessibility_provider_(std::make_unique<AccessibilityProvider>(
          std::move(accessibility_provider_proxy))),
      apps_access_manager_(std::make_unique<AppsAccessManagerImpl>(
          eche_connector_.get(),
          message_receiver_.get(),
          feature_status_provider_.get(),
          pref_service,
          multidevice_setup_client,
          connection_manager_.get())),
      eche_tray_stream_status_observer_(
          std::make_unique<EcheTrayStreamStatusObserver>(
              stream_status_change_handler_.get(),
              feature_status_provider_.get())),
      eche_stream_orientation_observer_(
          std::make_unique<EcheStreamOrientationObserver>()),
      eche_keyboard_layout_handler_(
          std::make_unique<EcheKeyboardLayoutHandler>()) {
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  system_info_provider_ = std::make_unique<SystemInfoProvider>(
      std::move(system_info), remote_cros_network_config_.get());
  // assign system_info_provider_ to eche signaler
  signaler_->SetSystemInfoProvider(system_info_provider_.get());

  if (features::IsEcheNetworkConnectionStateEnabled()) {
    phone_hub_manager_->SetEcheConnectionStatusHandler(
        eche_connection_status_handler_.get());
    phone_hub_manager_->SetSystemInfoProvider(system_info_provider_.get());
  }
}

EcheAppManager::~EcheAppManager() = default;

void EcheAppManager::BindSignalingMessageExchangerInterface(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  signaler_->Bind(std::move(receiver));
}

void EcheAppManager::BindSystemInfoProviderInterface(
    mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {
  system_info_provider_->Bind(std::move(receiver));
}

void EcheAppManager::BindAccessibilityProviderInterface(
    mojo::PendingReceiver<mojom::AccessibilityProvider> receiver) {
  accessibility_provider_->Bind(std::move(receiver));
}

void EcheAppManager::BindUidGeneratorInterface(
    mojo::PendingReceiver<mojom::UidGenerator> receiver) {
  uid_->Bind(std::move(receiver));
}

void EcheAppManager::BindNotificationGeneratorInterface(
    mojo::PendingReceiver<mojom::NotificationGenerator> receiver) {
  alert_generator_->Bind(std::move(receiver));
}

void EcheAppManager::BindDisplayStreamHandlerInterface(
    mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver) {
  stream_status_change_handler_->Bind(std::move(receiver));
}

void EcheAppManager::BindStreamOrientationObserverInterface(
    mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver) {
  eche_stream_orientation_observer_->Bind(std::move(receiver));
}

void EcheAppManager::BindConnectionStatusObserverInterface(
    mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver) {
  eche_connection_status_handler_->Bind(std::move(receiver));
}

void EcheAppManager::BindKeyboardLayoutHandlerInterface(
    mojo::PendingReceiver<mojom::KeyboardLayoutHandler> receiver) {
  eche_keyboard_layout_handler_->Bind(std::move(receiver));
}

AppsAccessManager* EcheAppManager::GetAppsAccessManager() {
  return apps_access_manager_.get();
}

EcheConnectionStatusHandler* EcheAppManager::GetEcheConnectionStatusHandler() {
  return eche_connection_status_handler_.get();
}

void EcheAppManager::CloseStream() {
  stream_status_change_handler_->CloseStream();
  accessibility_provider_->HandleStreamClosed();
}

void EcheAppManager::StreamGoBack() {
  stream_status_change_handler_->StreamGoBack();
}

void EcheAppManager::BubbleShown(AshWebView* view) {
  accessibility_provider_->TrackView(view);
}

// NOTE: These should be destroyed in the opposite order of how these objects
// are initialized in the constructor.
void EcheAppManager::Shutdown() {
  if (features::IsEcheNetworkConnectionStateEnabled() && phone_hub_manager_) {
    phone_hub_manager_->SetEcheConnectionStatusHandler(nullptr);
    phone_hub_manager_->SetSystemInfoProvider(nullptr);
  }

  eche_keyboard_layout_handler_.reset();
  eche_stream_orientation_observer_.reset();
  system_info_provider_.reset();
  eche_tray_stream_status_observer_.reset();
  apps_access_manager_.reset();
  accessibility_provider_.reset();
  alert_generator_.reset();
  eche_recent_app_click_handler_.reset();
  uid_.reset();
  eche_presence_manager_.reset();
  message_receiver_.reset();
  signaler_.reset();
  eche_connector_.reset();
  connection_scheduler_.reset();
  eche_notification_click_handler_.reset();
  stream_status_change_handler_.reset();
  apps_launch_info_provider_.reset();
  launch_app_helper_.reset();
  eche_connection_status_handler_.reset();
  feature_status_provider_.reset();
  eche_connection_status_handler_.reset();
  connection_manager_.reset();
}

}  // namespace eche_app
}  // namespace ash
