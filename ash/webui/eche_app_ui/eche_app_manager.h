// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_APP_MANAGER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_APP_MANAGER_H_

#include <stdint.h>

#include <memory>

#include "ash/public/cpp/ash_web_view.h"
#include "ash/webui/eche_app_ui/accessibility_provider.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"
#include "ash/webui/eche_app_ui/eche_notification_click_handler.h"
#include "ash/webui/eche_app_ui/eche_recent_app_click_handler.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace phonehub {
class PhoneHubManager;
}

namespace secure_channel {
class ConnectionManager;
class SecureChannelClient;
}  // namespace secure_channel

namespace eche_app {

class AppsLaunchInfoProvider;
class EcheConnector;
class EcheMessageReceiver;
class EcheAlertGenerator;
class EchePresenceManager;
class EcheSignaler;
class EcheUidProvider;
class SystemInfo;
class SystemInfoProvider;
class AppsAccessManager;
class EcheStreamStatusChangeHandler;
class EcheTrayStreamStatusObserver;
class EcheConnectionScheduler;
class EcheStreamOrientationObserver;
class EcheConnectionStatusHandler;
class EcheKeyboardLayoutHandler;

// Implements the core logic of the EcheApp and exposes interfaces via its
// public API. Implemented as a KeyedService since it depends on other
// KeyedService instances.
class EcheAppManager : public KeyedService {
 public:
  EcheAppManager(PrefService* pref_service,
                 std::unique_ptr<SystemInfo> system_info,
                 phonehub::PhoneHubManager*,
                 device_sync::DeviceSyncClient*,
                 multidevice_setup::MultiDeviceSetupClient*,
                 secure_channel::SecureChannelClient*,
                 std::unique_ptr<secure_channel::PresenceMonitorClient>
                     presence_monitor_client,
                 std::unique_ptr<AccessibilityProviderProxy>,
                 LaunchAppHelper::LaunchEcheAppFunction,
                 LaunchAppHelper::LaunchNotificationFunction,
                 LaunchAppHelper::CloseNotificationFunction);
  ~EcheAppManager() override;

  EcheAppManager(const EcheAppManager&) = delete;
  EcheAppManager& operator=(const EcheAppManager&) = delete;

  void BindSignalingMessageExchangerInterface(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  void BindUidGeneratorInterface(
      mojo::PendingReceiver<mojom::UidGenerator> receiver);

  void BindSystemInfoProviderInterface(
      mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

  void BindAccessibilityProviderInterface(
      mojo::PendingReceiver<mojom::AccessibilityProvider> receiver);

  void BindNotificationGeneratorInterface(
      mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

  void BindDisplayStreamHandlerInterface(
      mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver);

  void BindStreamOrientationObserverInterface(
      mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver);

  void BindConnectionStatusObserverInterface(
      mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver);

  void BindKeyboardLayoutHandlerInterface(
      mojo::PendingReceiver<mojom::KeyboardLayoutHandler> receiver);

  AppsAccessManager* GetAppsAccessManager();

  EcheConnectionStatusHandler* GetEcheConnectionStatusHandler();

  // This trigger Eche Web to release connection resource.
  void CloseStream();

  // This trigger Eche Web to go back the previous page.
  void StreamGoBack();

  // This is triggered when the app bubble appears in the UI.
  void BubbleShown(AshWebView* view);

  // KeyedService:
  void Shutdown() override;

 private:
  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_;
  std::unique_ptr<secure_channel::ConnectionManager> connection_manager_;
  std::unique_ptr<EcheConnectionStatusHandler> eche_connection_status_handler_;
  std::unique_ptr<EcheFeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  std::unique_ptr<EcheStreamStatusChangeHandler> stream_status_change_handler_;
  std::unique_ptr<EcheNotificationClickHandler>
      eche_notification_click_handler_;
  std::unique_ptr<EcheConnectionScheduler> connection_scheduler_;
  std::unique_ptr<EcheConnector> eche_connector_;
  std::unique_ptr<EcheSignaler> signaler_;
  std::unique_ptr<EcheMessageReceiver> message_receiver_;
  std::unique_ptr<EchePresenceManager> eche_presence_manager_;
  std::unique_ptr<EcheUidProvider> uid_;
  std::unique_ptr<EcheRecentAppClickHandler> eche_recent_app_click_handler_;
  std::unique_ptr<EcheAlertGenerator> alert_generator_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::unique_ptr<SystemInfoProvider> system_info_provider_;
  std::unique_ptr<AccessibilityProvider> accessibility_provider_;
  std::unique_ptr<AppsAccessManager> apps_access_manager_;
  std::unique_ptr<EcheTrayStreamStatusObserver>
      eche_tray_stream_status_observer_;
  std::unique_ptr<EcheStreamOrientationObserver>
      eche_stream_orientation_observer_;
  std::unique_ptr<EcheKeyboardLayoutHandler> eche_keyboard_layout_handler_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_APP_MANAGER_H_
