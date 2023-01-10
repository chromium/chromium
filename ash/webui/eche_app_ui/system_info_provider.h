// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_

#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace eche_app {

extern const char kJsonDeviceNameKey[];
extern const char kJsonBoardNameKey[];
extern const char kJsonTabletModeKey[];
extern const char kJsonWifiConnectionStateKey[];
extern const char kJsonDebugModeKey[];
extern const char kJsonGaiaIdKey[];
extern const char kJsonDeviceTypeKey[];
extern const char kJsonMeasureLatencyKey[];
extern const char kJsonSendStartSignalingKey[];
extern const char kJsonDisableStunServerKey[];
extern const char kJsonCheckAndroidNetworkInfoKey[];

class SystemInfo;

// Provides the system information likes board/device names for EcheApp and
// exposes the interface via mojom.
class SystemInfoProvider
    : public mojom::SystemInfoProvider,
      public ScreenBacklightObserver,
      public TabletModeObserver,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  explicit SystemInfoProvider(
      std::unique_ptr<SystemInfo> system_info,
      chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config);
  ~SystemInfoProvider() override;

  SystemInfoProvider(const SystemInfoProvider&) = delete;
  SystemInfoProvider& operator=(const SystemInfoProvider&) = delete;

  std::string GetHashedWiFiSsid();
  // mojom::SystemInfoProvider:
  void GetSystemInfo(
      base::OnceCallback<void(const std::string&)> callback) override;
  void SetSystemInfoObserver(
      mojo::PendingRemote<mojom::SystemInfoObserver> observer) override;

  void Bind(mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

  // Fetches the hashed SSID of the WiFi network the device is currently
  // connected to.
  virtual void FetchWifiNetworkSsidHash();

  // Sends the value of is_different_network & android_device_on_cellular to a
  // remote observer on the TS/JS layer via mojo.
  void SetAndroidDeviceNetworkInfoChanged(bool is_different_network,
                                          bool android_device_on_cellular);

 protected:
  SystemInfoProvider();
  std::string hashed_wifi_ssid_;

 private:
  friend class SystemInfoProviderTest;

  // ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ash::ScreenBacklightState screen_state) override;
  // TabletModeObserver overrides.
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  void SetTabletModeChanged(bool enabled);

  // network_config::CrosNetworkConfigObserver overrides:
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;

  // Callback invoked from within FetchWifiNetworkSsidHash() that produces a
  // list of networks.
  void OnWifiNetworkListSsidFetch(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void FetchWifiNetworkList();
  void OnWifiNetworkList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  mojo::Receiver<mojom::SystemInfoProvider> info_receiver_{this};
  mojo::Remote<mojom::SystemInfoObserver> observer_remote_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_receiver_{this};
  std::unique_ptr<SystemInfo> system_info_;
  chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config_;
  chromeos::network_config::mojom::ConnectionStateType wifi_connection_state_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_
