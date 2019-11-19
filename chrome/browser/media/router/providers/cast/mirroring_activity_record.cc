// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity_record.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/media_router/route_request_result.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"

using blink::mojom::PresentationConnectionMessagePtr;
using cast_channel::Result;
using media_router::mojom::MediaRouteProvider;
using media_router::mojom::MediaRouter;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionParameters;
using mirroring::mojom::SessionType;

namespace media_router {

MirroringActivityRecord::MirroringActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker,
    int target_tab_id,
    const CastSinkExtraData& cast_data,
    mojom::MediaRouter* media_router,
    OnStopCallback callback)
    : ActivityRecord(route, app_id, message_handler, session_tracker),
      channel_id_(cast_data.cast_channel_id),
      // TODO(jrw): MirroringType::kOffscreenTab should be a possible value here
      // once the Presentation API 1UA mode is supported.
      mirroring_type_(target_tab_id == -1 ? MirroringType::kDesktop
                                          : MirroringType::kTab),
      on_stop_(std::move(callback)) {
  // TODO(jrw): Detect and report errors.

  mirroring_tab_id_ = target_tab_id;

  // Get a reference to the mirroring service host.
  switch (mirroring_type_) {
    case MirroringType::kDesktop: {
      auto stream_id = route.media_source().DesktopStreamId();
      DCHECK(stream_id);
      media_router->GetMirroringServiceHostForDesktop(
          /* tab_id */ -1, *stream_id, host_.BindNewPipeAndPassReceiver());
      break;
    }
    case MirroringType::kTab:
      media_router->GetMirroringServiceHostForTab(
          target_tab_id, host_.BindNewPipeAndPassReceiver());
      break;
    default:
      NOTREACHED();
  }

  // Bind Mojo receivers for the interfaces this object implements.
  mojo::PendingRemote<mirroring::mojom::SessionObserver> observer_remote;
  observer_receiver_.Bind(observer_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mirroring::mojom::CastMessageChannel> channel_remote;
  channel_receiver_.Bind(channel_remote.InitWithNewPipeAndPassReceiver());

  // Derive session type from capabilities.
  const bool has_audio = (cast_data.capabilities &
                          static_cast<uint8_t>(cast_channel::AUDIO_OUT)) != 0;
  const bool has_video = (cast_data.capabilities &
                          static_cast<uint8_t>(cast_channel::VIDEO_OUT)) != 0;
  DCHECK(has_audio || has_video);
  const SessionType session_type =
      has_audio && has_video
          ? SessionType::AUDIO_AND_VIDEO
          : has_audio ? SessionType::AUDIO_ONLY : SessionType::VIDEO_ONLY;

  // Arrange to start mirroring once the session is set.
  on_session_set_ = base::BindOnce(
      &mirroring::mojom::MirroringServiceHost::Start,
      base::Unretained(host_.get()),
      SessionParameters::New(session_type, cast_data.ip_endpoint.address(),
                             cast_data.model_name),
      std::move(observer_remote), std::move(channel_remote),
      channel_to_service_.BindNewPipeAndPassReceiver());
}

MirroringActivityRecord::~MirroringActivityRecord() = default;

void MirroringActivityRecord::OnError(SessionError error) {
  DLOG(ERROR) << "Mirroring session error: " << error;
  StopMirroring();
}

void MirroringActivityRecord::DidStart() {
  base::UmaHistogramEnumeration("MediaRouter.CastStreaming.Start.Success",
                                mirroring_type_);
}

void MirroringActivityRecord::DidStop() {
  base::RecordAction(
      base::UserMetricsAction("MediaRouter.CastStreaming.Session.End"));
  StopMirroring();
}

void MirroringActivityRecord::Send(mirroring::mojom::CastMessagePtr message) {
  DCHECK(message);
  DVLOG(2) << "Relaying message to receiver: " << message->json_format_data;

  GetDataDecoder().ParseJson(
      message->json_format_data,
      base::BindRepeating(
          [](const std::string& route_id,
             base::WeakPtr<MirroringActivityRecord> self,
             data_decoder::DataDecoder::ValueOrError result) {
            if (!result.value) {
              // TODO(crbug.com/905002): Record UMA metric for parse result.
              DLOG(ERROR) << "Failed to parse Cast client message for "
                          << route_id << ": " << *result.error;
              return;
            }

            if (!self)
              return;

            CastSession* session = self->GetSession();
            DCHECK(session);

            // TODO(jrw): Can some of this logic be shared with
            // CastActivityRecord::SendAppMessageToReceiver?
            cast_channel::CastMessage cast_message =
                cast_channel::CreateCastMessage(
                    mirroring::mojom::kWebRtcNamespace,
                    std::move(*result.value),
                    self->message_handler_->sender_id(),
                    session->transport_id());
            self->message_handler_->SendCastMessage(self->channel_id_,
                                                    cast_message);
          },
          route().media_route_id(), weak_ptr_factory_.GetWeakPtr()));
}

Result MirroringActivityRecord::SendAppMessageToReceiver(
    const CastInternalMessage& cast_message) {
  // This method is only called from CastSessionClient.
  NOTREACHED();
  return Result::kOk;
}

base::Optional<int> MirroringActivityRecord::SendMediaRequestToReceiver(
    const CastInternalMessage& cast_message) {
  // This method is only called from CastSessionClient.
  NOTREACHED();
  return base::nullopt;
}

void MirroringActivityRecord::SendSetVolumeRequestToReceiver(
    const CastInternalMessage& cast_message,
    cast_channel::ResultCallback callback) {
  // This method is only called from CastSessionClient.
  //
  // Comment from tamumif: This method may become relevant when we are adding
  // global media controls support, but the current plan is to not show
  // mirroring sessions in global media controls, in which case we don't need
  // this. I think the implementation is shared between CastActivityRecord and
  // MirroringActivityRecord, so it could be put in the ActivityRecord base
  // class if we wanted to.
  NOTREACHED();
}

void MirroringActivityRecord::SendStopSessionMessageToReceiver(
    const base::Optional<std::string>& client_id,
    const std::string& hash_token,
    MediaRouteProvider::TerminateRouteCallback callback) {
  // TODO(jrw): What, if anything, should happen here?
  std::move(callback).Run(base::nullopt, RouteRequestResult::ResultCode::OK);
}

void MirroringActivityRecord::HandleLeaveSession(const std::string& client_id) {
  // This method is only called from CastSessionClient.
  NOTREACHED();
}

mojom::RoutePresentationConnectionPtr MirroringActivityRecord::AddClient(
    const CastMediaSource& source,
    const url::Origin& origin,
    int tab_id) {
  // This method seems to only be called on CastActivityRecord instances.
  NOTREACHED();
  return nullptr;
}

void MirroringActivityRecord::RemoveClient(const std::string& client_id) {
  // This method is never called, and it should probably only ever be called on
  // CastActivityRecord instances.
  NOTREACHED();
}

void MirroringActivityRecord::SendMessageToClient(
    const std::string& client_id,
    PresentationConnectionMessagePtr message) {}

void MirroringActivityRecord::SendMediaStatusToClients(
    const base::Value& media_status,
    base::Optional<int> request_id) {}

void MirroringActivityRecord::ClosePresentationConnections(
    blink::mojom::PresentationConnectionCloseReason close_reason) {}

void MirroringActivityRecord::TerminatePresentationConnections() {}

void MirroringActivityRecord::OnAppMessage(
    const cast_channel::CastMessage& message) {
  if (message.namespace_() != mirroring::mojom::kWebRtcNamespace &&
      message.namespace_() == mirroring::mojom::kRemotingNamespace) {
    // Ignore message with wrong namespace.
    return;
  }
  DVLOG(2) << "Relaying app message from receiver: " << message;
  DCHECK(message.has_payload_utf8());
  DCHECK_EQ(message.protocol_version(),
            cast_channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.namespace_();
  ptr->json_format_data = message.payload_utf8();
  // TODO(jrw): Do something with message.source_id() and
  // message.destination_id()?
  channel_to_service_->Send(std::move(ptr));
}

void MirroringActivityRecord::OnInternalMessage(
    const cast_channel::InternalMessage& message) {
  DVLOG(2) << "Relaying internal message from receiver: " << message.message;
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.message_namespace;

  // TODO(jrw): This line re-serializes a JSON string that was parsed by the
  // caller of this method.  Yuck!  This is probably a necessary evil as long as
  // the extension needs to communicate with the mirroring service.
  CHECK(base::JSONWriter::Write(message.message, &ptr->json_format_data));

  channel_to_service_->Send(std::move(ptr));
}

void MirroringActivityRecord::CreateMediaController(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {}

void MirroringActivityRecord::StopMirroring() {
  // Running the callback will cause this object to be deleted.
  if (on_stop_)
    std::move(on_stop_).Run();
}

}  // namespace media_router
