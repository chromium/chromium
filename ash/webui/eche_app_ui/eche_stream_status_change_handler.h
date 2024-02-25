// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_STREAM_STATUS_CHANGE_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_STREAM_STATUS_CHANGE_HANDLER_H_

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace eche_app {

class AppsLaunchInfoProvider;

// Implements the DisplayStreamHandler interface to allow the WebUI to sync the
// status of the video streaming for Eche, e.g. When the video streaming is
// started in the Eche Web, we can register `Observer` and get this status via
// `OnStartStreaming` and `OnStreamStatusChanged` event.
class EcheStreamStatusChangeHandler
    : public mojom::DisplayStreamHandler,
      public EcheConnectionStatusHandler::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnStartStreaming() = 0;
    virtual void OnStreamStatusChanged(mojom::StreamStatus status) = 0;
  };

  EcheStreamStatusChangeHandler(
      AppsLaunchInfoProvider* apps_launch_info_provider,
      EcheConnectionStatusHandler* eche_connection_status_handler);
  ~EcheStreamStatusChangeHandler() override;

  EcheStreamStatusChangeHandler(const EcheStreamStatusChangeHandler&) = delete;
  EcheStreamStatusChangeHandler& operator=(
      const EcheStreamStatusChangeHandler&) = delete;

  // mojom::DisplayStreamHandler:
  void StartStreaming() override;
  void OnStreamStatusChanged(mojom::StreamStatus status) override;
  void SetStreamActionObserver(
      mojo::PendingRemote<mojom::StreamActionObserver> observer) override;

  // EcheConnectionStatusHandler::Observer:
  void OnRequestCloseConnection() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Bind(mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver);

  void CloseStream();
  void StreamGoBack();

 protected:
  void NotifyStartStreaming();
  void NotifyStreamStatusChanged(mojom::StreamStatus status);

 private:
  raw_ptr<AppsLaunchInfoProvider, DanglingUntriaged> apps_launch_info_provider_;
  raw_ptr<EcheConnectionStatusHandler> eche_connection_status_handler_;
  mojo::Receiver<mojom::DisplayStreamHandler> display_stream_receiver_{this};
  mojo::Remote<mojom::StreamActionObserver> observer_remote_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_STREAM_STATUS_CHANGE_HANDLER_H_
