// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_STATUS_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_STATUS_HANDLER_H_

#include "ash/webui/eche_app_ui/feature_status.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::eche_app {

// Implements the ConnectionStatusObserver interface to receive the connection
// status when attempting to bootstrap the connection to the phone.
class EcheConnectionStatusHandler : public mojom::ConnectionStatusObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Includes connection status changes for all connections (app stream +
    // pre-warm).
    virtual void OnConnectionStatusChanged(
        mojom::ConnectionStatus connection_status);

    // For determining when app streaming is allowed in UI.
    virtual void OnConnectionStatusForUiChanged(
        mojom::ConnectionStatus connection_status);

    virtual void OnRequestBackgroundConnectionAttempt();

    virtual void OnRequestCloseConnection();

    virtual void OnConnectionClosed();
  };

  EcheConnectionStatusHandler();
  ~EcheConnectionStatusHandler() override;

  EcheConnectionStatusHandler(const EcheConnectionStatusHandler&) = delete;
  EcheConnectionStatusHandler& operator=(const EcheConnectionStatusHandler&) =
      delete;

  // mojom::ConnectionStatusObserver:
  void OnConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Checks on the status of the connection to be used for deciding whether app
  // streaming should be allowed on the current connection. Triggers
  // OnConnectionStatusForUiChanged().
  void CheckConnectionStatusForUi();
  void SetConnectionStatusForUi(mojom::ConnectionStatus connection_status);

  // TODO(b/274530047): Refactor to make this a real observer / actually
  // override.
  // EcheFeatureStatusProvider::Observer:
  void OnFeatureStatusChanged(FeatureStatus feature_status);

  // Proxy to request that the webui shut down the connection.
  void NotifyRequestCloseConnection();

  void NotifyConnectionClosed();

  void Bind(mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver);

  mojom::ConnectionStatus connection_status_for_ui() const {
    return connection_status_for_ui_;
  }

 private:
  friend class EcheConnectionStatusHandlerTest;

  void NotifyConnectionStatusChanged(mojom::ConnectionStatus connection_status);
  void NotifyConnectionStatusForUiChanged(
      mojom::ConnectionStatus connection_status);
  void NotifyRequestBackgroundConnectionAttempt();

  void TriggerBackgroundConnectionIfNecessary();
  void ResetConnectionStatus();

  void set_feature_status_for_test(FeatureStatus feature_status) {
    feature_status_ = feature_status;
  }
  bool is_connecting_or_connected_for_test() {
    return is_connecting_or_connected_;
  }

  mojom::ConnectionStatus connection_status_for_ui_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
  base::Time last_update_timestamp_ = base::Time();

  // Tracks the current status of the eche connection (app stream or
  // background).
  bool is_connecting_or_connected_ = false;

  std::unique_ptr<base::OneShotTimer> status_check_delay_timer_{};
  FeatureStatus feature_status_ = FeatureStatus::kDisconnected;
  mojo::Receiver<mojom::ConnectionStatusObserver> connection_status_receiver_{
      this};
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_STATUS_HANDLER_H_
