// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/providers/cast/cast_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_router.mojom-forward.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace media_router {

struct CastSinkExtraData;

class MirroringActivity : public CastActivity,
                          public mirroring::mojom::SessionObserver,
                          public mirroring::mojom::CastMessageChannel {
 public:
  using OnStopCallback = base::OnceClosure;

  enum class MirroringType {
    kTab,           // Mirror a single tab.
    kDesktop,       // Mirror the whole desktop.
    kOffscreenTab,  // Used for Presentation API 1UA mode.
    kMaxValue = kOffscreenTab,
  };

  MirroringActivity(const MediaRoute& route,
                    const std::string& app_id,
                    cast_channel::CastMessageHandler* message_handler,
                    CastSessionTracker* session_tracker,
                    int target_tab_id,
                    const CastSinkExtraData& cast_data,
                    OnStopCallback callback);
  ~MirroringActivity() override;

  virtual void CreateMojoBindings(mojom::MediaRouter* media_router);

  // SessionObserver implementation
  void OnError(mirroring::mojom::SessionError error) override;
  void DidStart() override;
  void DidStop() override;
  void LogInfoMessage(const std::string& message) override;
  void LogErrorMessage(const std::string& message) override;

  // CastMessageChannel implementation
  void Send(mirroring::mojom::CastMessagePtr message) override;

  // CastActivity implementation
  void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) override;
  void OnAppMessage(const cast::channel::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;

 protected:
  void OnSessionSet(const CastSession& session) override;
  void CreateMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, GetScrubbedLogMessage);

  void HandleParseJsonResult(const std::string& route_id,
                             data_decoder::DataDecoder::ValueOrError result);

  void StopMirroring();

  // Scrubs AES related data in messages with type "OFFER".
  static std::string GetScrubbedLogMessage(const base::Value& message);

  mojo::Remote<mirroring::mojom::MirroringServiceHost> host_;

  // Sends Cast messages from the mirroring receiver to the mirroring service.
  mojo::Remote<mirroring::mojom::CastMessageChannel> channel_to_service_;

  // Only used to store pending CastMessageChannel receiver while waiting for
  // OnSessionSet() to be called.
  mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
      channel_to_service_receiver_;

  // Remote to the logger owned by the Media Router. Used to log WebRTC messages
  // sent between the mirroring service and mirroring receiver.
  // |logger_| should be bound before the CastMessageChannel message pipe is
  // created.
  mojo::Remote<mojom::Logger> logger_;

  mojo::Receiver<mirroring::mojom::SessionObserver> observer_receiver_{this};

  // To handle Cast messages from the mirroring service to the mirroring
  // receiver.
  mojo::Receiver<mirroring::mojom::CastMessageChannel> channel_receiver_{this};

  // Set before and after a mirroring session is established, for metrics.
  base::Optional<base::Time> will_start_mirroring_timestamp_;
  base::Optional<base::Time> did_start_mirroring_timestamp_;

  const base::Optional<MirroringType> mirroring_type_;
  const CastSinkExtraData cast_data_;
  OnStopCallback on_stop_;
  base::WeakPtrFactory<MirroringActivity> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_
