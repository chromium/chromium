// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_SIGNALER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_SIGNALER_H_

#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "ash/webui/eche_app_ui/eche_connector.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace eche_app {

class EcheSignaler : public mojom::SignalingMessageExchanger,
                     public secure_channel::ConnectionManager::Observer {
 public:
  EcheSignaler(EcheConnector* eche_connector,
               secure_channel::ConnectionManager* connection_manager);
  ~EcheSignaler() override;

  EcheSignaler(const EcheSignaler&) = delete;
  EcheSignaler& operator=(const EcheSignaler&) = delete;

  // mojom::SignalingMessageExchanger:
  void SendSignalingMessage(const std::vector<uint8_t>& signal) override;
  void SetSignalingMessageObserver(
      mojo::PendingRemote<mojom::SignalingMessageObserver> observer) override;
  void TearDownSignaling() override;

  void Bind(mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  // Visible for testing.
  // secure_channel::ConnectionManager::Observer:
  void OnMessageReceived(const std::string& payload) override;

 private:
  EcheConnector* eche_connector_;
  secure_channel::ConnectionManager* connection_manager_;
  mojo::Remote<mojom::SignalingMessageObserver> observer_;
  mojo::Receiver<mojom::SignalingMessageExchanger> exchanger_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_SIGNALER_H_
