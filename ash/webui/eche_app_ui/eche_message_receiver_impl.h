// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_IMPL_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_IMPL_H_

#include "ash/webui/eche_app_ui/eche_message_receiver.h"
#include "base/memory/raw_ptr.h"

#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace eche_app {

class EcheMessageReceiverImpl
    : public EcheMessageReceiver,
      public secure_channel::ConnectionManager::Observer {
 public:
  explicit EcheMessageReceiverImpl(
      secure_channel::ConnectionManager* connection_manager);
  ~EcheMessageReceiverImpl() override;

 private:
  friend class FakeEcheMessageReceiver;
  // secure_channel::ConnectionManager::Observer:
  void OnMessageReceived(const std::string& payload) override;

  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_IMPL_H_
