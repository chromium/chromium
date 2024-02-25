// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_IMPL_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_IMPL_H_

#include "ash/webui/eche_app_ui/eche_connector.h"
#include "base/memory/raw_ptr.h"

#include "ash/webui/eche_app_ui/eche_connection_scheduler.h"
#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/containers/queue.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace eche_app {

// Connects to target device when a message is made available to send (queuing
// messages if the connection is not yet ready), and disconnects (dropping all
// pending messages) when requested.
class EcheConnectorImpl : public EcheConnector,
                          public FeatureStatusProvider::Observer,
                          public secure_channel::ConnectionManager::Observer {
 public:
  EcheConnectorImpl(FeatureStatusProvider* eche_feature_status_provider,
                    secure_channel::ConnectionManager* connection_manager,
                    EcheConnectionScheduler* connection_scheduler);
  ~EcheConnectorImpl() override;

  void SendMessage(const proto::ExoMessage message) override;
  void Disconnect() override;
  void SendAppsSetupRequest() override;
  void GetAppsAccessStateRequest() override;
  void AttemptNearbyConnection() override;
  int GetMessageCount();

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  void QueueMessageWhenDisabled(const proto::ExoMessage message);
  bool IsMessageAllowedWhenDisabled(const proto::ExoMessage message);
  void MaybeFlushQueue();
  void FlushQueue();
  void FlushQueueWhenDisabled();

  raw_ptr<FeatureStatusProvider> eche_feature_status_provider_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<EcheConnectionScheduler> connection_scheduler_;
  base::queue<proto::ExoMessage> message_queue_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_IMPL_H_
