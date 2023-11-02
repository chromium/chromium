// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_H_

#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace eche_app {

class EcheMessageReceiver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the apps access state response sent by the remote phone.
    virtual void OnGetAppsAccessStateResponseReceived(
        proto::GetAppsAccessStateResponse apps_access_state_response) = 0;

    // Called when the apps setup response sent by the remote phone.
    virtual void OnSendAppsSetupResponseReceived(
        proto::SendAppsSetupResponse apps_setup_response) = 0;

    virtual void OnStatusChange(proto::StatusChangeType status_change_type) = 0;

    // Called when the app policy state changed sent by the remote phone.
    virtual void OnAppPolicyStateChange(
        proto::AppStreamingPolicy app_policy_state) = 0;
  };

  EcheMessageReceiver(const EcheMessageReceiver&) = delete;
  EcheMessageReceiver& operator=(const EcheMessageReceiver&) = delete;
  virtual ~EcheMessageReceiver();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  EcheMessageReceiver();
  void NotifyGetAppsAccessStateResponse(
      proto::GetAppsAccessStateResponse apps_access_state_response);
  void NotifySendAppsSetupResponse(
      proto::SendAppsSetupResponse apps_setup_response);
  void NotifyStatusChange(proto::StatusChange status_change);
  void NotifyAppPolicyStateChange(proto::PolicyStateChange policy_state_change);

 private:
  friend class FakeEcheMessageReceiver;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_H_
