// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_DISPLAY_STREAM_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_DISPLAY_STREAM_HANDLER_H_

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace eche_app {

// Implements the EcheDisplayStreamHandler interface to allow the WebUI to sync
// the status of the display stream for Eche, e.g. When the display stream is
// started  in the Eche Web, we can register `Observer` and get this status via
// `OnStartStreaming` event.
// TODO(paulzchen): Consider using `DisplayStreamEventHandler` to replace
// `DisplayStreamHandler`.
class EcheDisplayStreamHandler : public mojom::DisplayStreamHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    //  Called when the streaming is ready. About another status:
    //  OnStopStreaming, we prefer to listen to the stop signal when the bubble
    //  is really closed.
    // TODO(paulzchen): Using generic method `OnStreamStatusChanged`.
    virtual void OnStartStreaming() = 0;
  };

  EcheDisplayStreamHandler();
  ~EcheDisplayStreamHandler() override;

  EcheDisplayStreamHandler(const EcheDisplayStreamHandler&) = delete;
  EcheDisplayStreamHandler& operator=(const EcheDisplayStreamHandler&) = delete;

  // mojom::DisplayStreamHandler:
  void StartStreaming() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Bind(mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver);

 protected:
  void NotifyStartStreaming();

 private:
  mojo::Receiver<mojom::DisplayStreamHandler> display_stream_receiver_{this};
  base::ObserverList<Observer> observer_list_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_DISPLAY_STREAM_HANDLER_H_
