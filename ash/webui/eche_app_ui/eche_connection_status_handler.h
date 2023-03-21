// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_STATUS_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_STATUS_HANDLER_H_

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

    virtual void OnConnectionStatusChanged(
        mojom::ConnectionStatus connection_status) = 0;
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

  void Bind(mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver);

 protected:
  void NotifyConnectionStatusChanged(mojom::ConnectionStatus connection_status);

 private:
  mojo::Receiver<mojom::ConnectionStatusObserver> connection_status_receiver_{
      this};
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_STATUS_HANDLER_H_
