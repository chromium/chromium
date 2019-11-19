// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/providers/cast/activity_record.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_router.mojom-forward.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

struct CastSinkExtraData;

class MirroringActivityRecord : public ActivityRecord,
                                public mirroring::mojom::SessionObserver,
                                public mirroring::mojom::CastMessageChannel {
 public:
  using OnStopCallback = base::OnceClosure;

  MirroringActivityRecord(const MediaRoute& route,
                          const std::string& app_id,
                          cast_channel::CastMessageHandler* message_handler,
                          CastSessionTracker* session_tracker,
                          int target_tab_id,
                          const CastSinkExtraData& cast_data,
                          mojom::MediaRouter* media_router,
                          OnStopCallback callback);
  ~MirroringActivityRecord() override;

  // SessionObserver implementation
  void OnError(mirroring::mojom::SessionError error) override;
  void DidStart() override;
  void DidStop() override;

  // CastMessageChannel implementation
  void Send(mirroring::mojom::CastMessagePtr message) override;

  // ActivityRecord implementation
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) override;
  base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) override;
  void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) override;
  void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      const std::string& hash_token,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) override;
  void HandleLeaveSession(const std::string& client_id) override;
  mojom::RoutePresentationConnectionPtr AddClient(const CastMediaSource& source,
                                                  const url::Origin& origin,
                                                  int tab_id) override;
  void RemoveClient(const std::string& client_id) override;
  void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) override;
  void SendMediaStatusToClients(const base::Value& media_status,
                                base::Optional<int> request_id) override;
  void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason) override;
  void TerminatePresentationConnections() override;
  void OnAppMessage(const cast_channel::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;

 protected:
  void CreateMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;

 private:
  enum class MirroringType {
    kTab,           // Mirror a single tab.
    kDesktop,       // Mirror the whole desktop.
    kOffscreenTab,  // Used for Presentation API 1UA mode.
    kMaxValue = kOffscreenTab,
  };

  void StopMirroring();

  mojo::Remote<mirroring::mojom::MirroringServiceHost> host_;

  // Sends Cast messages from the mirroring receiver to the mirroring service.
  mojo::Remote<mirroring::mojom::CastMessageChannel> channel_to_service_;

  mojo::Receiver<mirroring::mojom::SessionObserver> observer_receiver_{this};

  // To handle Cast messages from the mirroring service to the mirroring
  // receiver.
  mojo::Receiver<mirroring::mojom::CastMessageChannel> channel_receiver_{this};

  const int channel_id_;
  const MirroringType mirroring_type_;
  OnStopCallback on_stop_;
  base::WeakPtrFactory<MirroringActivityRecord> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_
