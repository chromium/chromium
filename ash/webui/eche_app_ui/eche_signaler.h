// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_SIGNALER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_SIGNALER_H_

#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/eche_connector.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/webui/eche_app_ui/system_info_provider.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::eche_app {

class EcheSignaler : public mojom::SignalingMessageExchanger,
                     public secure_channel::ConnectionManager::Observer,
                     public EcheConnectionStatusHandler::Observer {
 public:
  EcheSignaler(EcheConnector* eche_connector,
               secure_channel::ConnectionManager* connection_manager,
               AppsLaunchInfoProvider* apps_launch_info_provider,
               EcheConnectionStatusHandler* eche_connection_status_handler);
  ~EcheSignaler() override;

  EcheSignaler(const EcheSignaler&) = delete;
  EcheSignaler& operator=(const EcheSignaler&) = delete;

  // mojom::SignalingMessageExchanger:
  void SendSignalingMessage(const std::vector<uint8_t>& signal) override;
  void SetSignalingMessageObserver(
      mojo::PendingRemote<mojom::SignalingMessageObserver> observer) override;
  void SetSystemInfoProvider(SystemInfoProvider* system_info_provider);
  void TearDownSignaling() override;

  void Bind(mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  // Visible for testing.
  // secure_channel::ConnectionManager::Observer:
  void OnMessageReceived(const std::string& payload) override;

  base::DelayTimer* signaling_timeout_timer_for_test() {
    return signaling_timeout_timer_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenNoReceiveAnyMessage);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenSignalingHasLateRequest);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenSignalingHasLateResponse);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenSecurityChannelDisconnected);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenWiFiNetworksDifferent);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenWiFiNetworksSame);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           TestConnectionFailWhenRemoteDeviceOnCellular);
  FRIEND_TEST_ALL_PREFIXES(EcheSignalerTest,
                           OnRequestCloseConnectionDoesNotStreamEventFailures);

  // EcheConnectionStatusHandler::Observer
  void OnConnectionClosed() override;

  void RecordSignalingTimeout();
  void ProcessAndroidNetworkInfo(const proto::ExoMessage& message);

  // The signaling timer to log fail reason in case response timeout.
  std::unique_ptr<base::DelayTimer> signaling_timeout_timer_;

  // This is for identify the timeout reason.
  EcheTray::ConnectionFailReason probably_connection_failed_reason_ =
      EcheTray::ConnectionFailReason::kUnknown;

  raw_ptr<SystemInfoProvider, DanglingUntriaged> system_info_provider_ =
      nullptr;
  raw_ptr<EcheConnector> eche_connector_ = nullptr;
  raw_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_ = nullptr;
  raw_ptr<EcheConnectionStatusHandler> eche_connection_status_handler_ =
      nullptr;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_ = nullptr;
  mojo::Remote<mojom::SignalingMessageObserver> observer_;
  mojo::Receiver<mojom::SignalingMessageExchanger> exchanger_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         EcheTray::ConnectionFailReason connection_fail_reason);

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_SIGNALER_H_
