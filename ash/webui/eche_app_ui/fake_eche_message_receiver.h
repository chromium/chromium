// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FAKE_ECHE_MESSAGE_RECEIVER_H_
#define ASH_WEBUI_ECHE_APP_UI_FAKE_ECHE_MESSAGE_RECEIVER_H_

#include "ash/webui/eche_app_ui/eche_message_receiver.h"

namespace ash {
namespace eche_app {

class FakeEcheMessageReceiver : public EcheMessageReceiver {
 public:
  FakeEcheMessageReceiver();
  ~FakeEcheMessageReceiver() override;

  void FakeGetAppsAccessStateResponse(proto::Result result,
                                      proto::AppsAccessState status);
  void FakeSendAppsSetupResponse(proto::Result result,
                                 proto::AppsAccessState status);
  void FakeStatusChange(proto::StatusChangeType status_change_type);

  void FakeAppPolicyStateChange(proto::AppStreamingPolicy app_policy_state);

 private:
  using EcheMessageReceiver::NotifyAppPolicyStateChange;
  using EcheMessageReceiver::NotifyGetAppsAccessStateResponse;
  using EcheMessageReceiver::NotifySendAppsSetupResponse;
  using EcheMessageReceiver::NotifyStatusChange;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FAKE_ECHE_MESSAGE_RECEIVER_H_
