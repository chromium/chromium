// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_NETWORK_SETTINGS_NETWORK_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_NETWORK_SETTINGS_NETWORK_SETTINGS_SERVICE_H_

#include <string>

#include "chrome/browser/ash/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/cfm_network_settings.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

// Hotline service allowing CFMs to open the network settings dialog from Meet.
class NetworkSettingsService : public CfmObserver,
                               public ServiceAdaptor::Delegate,
                               public chromeos::cfm::mojom::CfmNetworkSettings {
 public:
  ~NetworkSettingsService() override;

  NetworkSettingsService(const NetworkSettingsService&) = delete;
  NetworkSettingsService& operator=(const NetworkSettingsService&) = delete;

  static void Initialize();
  static void Shutdown();
  static NetworkSettingsService* Get();
  static bool IsInitialized();

  // mojom::CfmNetworkSettings implementation
  void ShowDialog() override;

  // CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // ServiceAdaptorDelegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorDisconnect() override;

 private:
  NetworkSettingsService();

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::CfmNetworkSettings> receivers_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_NETWORK_SETTINGS_NETWORK_SETTINGS_SERVICE_H_
