// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_IMPL_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_IMPL_H_

#include "ash/webui/eche_app_ui/eche_connector.h"

#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/containers/queue.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace eche_app {

// Connects to target device when a message is made available to send (queuing
// messages if the connection is not yet ready), and disconnects (dropping all
// pending messages) when requested.
class EcheConnectorImpl : public EcheConnector,
                          public FeatureStatusProvider::Observer {
 public:
  EcheConnectorImpl(EcheFeatureStatusProvider* eche_feature_status_provider,
                    secure_channel::ConnectionManager* connection_manager);
  ~EcheConnectorImpl() override;

  void SendMessage(const std::string& message) override;
  void Disconnect() override;
  void SendAppsSetupRequest() override;
  void GetAppsAccessStateRequest() override;
  void AttemptNearbyConnection() override;

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  void FlushQueue();

  EcheFeatureStatusProvider* eche_feature_status_provider_;
  secure_channel::ConnectionManager* connection_manager_;
  base::queue<std::string> message_queue_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_IMPL_H_
