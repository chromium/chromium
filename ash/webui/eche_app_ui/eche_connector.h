// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_H_

#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/containers/queue.h"

namespace ash {
namespace eche_app {

// Provides interface to connect to target device when a message is made
// available to send (queuing messages if the connection is not yet ready), and
// disconnects (dropping all pending messages) when requested.
class EcheConnector {
 public:
  virtual ~EcheConnector() = default;

  virtual void SendMessage(const proto::ExoMessage message) = 0;
  virtual void Disconnect() = 0;
  virtual void SendAppsSetupRequest() = 0;
  virtual void GetAppsAccessStateRequest() = 0;
  virtual void AttemptNearbyConnection() = 0;

 protected:
  EcheConnector() = default;

 private:
  friend class FakeEcheConnector;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTOR_H_
