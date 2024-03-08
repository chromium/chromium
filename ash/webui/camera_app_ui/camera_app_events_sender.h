// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_EVENTS_SENDER_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_EVENTS_SENDER_H_

#include "ash/webui/camera_app_ui/events_sender.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class CameraAppEventsSender : public camera_app::mojom::EventsSender {
 public:
  explicit CameraAppEventsSender(std::string system_language);
  CameraAppEventsSender(const CameraAppEventsSender&) = delete;
  CameraAppEventsSender& operator=(const CameraAppEventsSender&) = delete;
  ~CameraAppEventsSender() override;

  // Creates the mojo connection, binds the receiver and returns the remote.
  mojo::PendingRemote<camera_app::mojom::EventsSender> CreateConnection();

  // camera_app::mojom::EventsSender implementations.
  void SendStartSessionEvent(
      camera_app::mojom::StartSessionEventParamsPtr params) override;

 private:
  bool CanSendEvents();

  std::string system_language_;

  mojo::ReceiverSet<camera_app::mojom::EventsSender> receivers_;
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_EVENTS_SENDER_H_
