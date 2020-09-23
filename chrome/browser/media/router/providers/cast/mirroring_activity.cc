// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"

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
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/enum_table.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "components/mirroring/mojom/session_parameters.mojom-forward.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using blink::mojom::PresentationConnectionMessagePtr;
using cast_channel::Result;
using media_router::mojom::MediaRouteProvider;
using media_router::mojom::MediaRouter;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionParameters;
using mirroring::mojom::SessionType;

namespace media_router {

namespace {

constexpr char kHistogramSessionLaunch[] =
    "MediaRouter.CastStreaming.Session.Launch";
constexpr char kHistogramSessionLength[] =
    "MediaRouter.CastStreaming.Session.Length";
constexpr char kHistogramSessionLengthScreen[] =
    "MediaRouter.CastStreaming.Session.Length.Screen";
constexpr char kHistogramSessionLengthFile[] =
    "MediaRouter.CastStreaming.Session.Length.File";
constexpr char kHistogramSessionLengthOffscreenTab[] =
    "MediaRouter.CastStreaming.Session.Length.OffscreenTab";
constexpr char kHistogramSessionLengthTab[] =
    "MediaRouter.CastStreaming.Session.Length.Tab";
constexpr char kHistogramStartFailureNative[] =
    "MediaRouter.CastStreaming.Start.Failure.Native";
constexpr char kHistogramStartSuccess[] =
    "MediaRouter.CastStreaming.Start.Success";

using MirroringType = MirroringActivity::MirroringType;

const std::string GetMirroringNamespace(const base::Value& message) {
  const base::Value* const type_value =
      message.FindKeyOfType("type", base::Value::Type::STRING);

  if (type_value &&
      type_value->GetString() ==
          cast_util::EnumToString<cast_channel::CastMessageType,
                                  cast_channel::CastMessageType::kRpc>()) {
    return mirroring::mojom::kRemotingNamespace;
  } else {
    return mirroring::mojom::kWebRtcNamespace;
  }
}

// Get the mirroring type for a media route.  Note that |target_tab_id| is
// usually ignored here, because mirroring typically only happens with a special
// URL that includes the tab ID it needs, which should be the same as the tab ID
// selected by the media router.
base::Optional<MirroringActivity::MirroringType> GetMirroringType(
    const MediaRoute& route,
    int target_tab_id) {
  if (!route.is_local())
    return base::nullopt;

  const auto source = route.media_source();
  if (source.IsTabMirroringSource() || source.IsLocalFileSource())
    return MirroringActivity::MirroringType::kTab;
  if (source.IsDesktopMirroringSource())
    return MirroringActivity::MirroringType::kDesktop;

  if (source.url().is_valid()) {
    if (source.IsCastPresentationUrl()) {
      const auto cast_source = CastMediaSource::FromMediaSource(source);
      if (cast_source && cast_source->ContainsStreamingApp()) {
        // This is a weird case.  Normally if the source is a presentation URL,
        // we use 2-UA mode rather than mirroring, but if the app ID it
        // specifies is one of the special streaming app IDs, we activate
        // mirroring instead. This only happens when a Cast SDK client requests
        // a mirroring app ID, which causes its own tab to be mirrored.  This is
        // a strange thing to do and it's not officially supported, but some
        // apps, like, Google Slides rely on it.  Unlike a proper tab-based
        // MediaSource, this kind of MediaSource doesn't specify a tab in the
        // URL, so we choose the tab that was active when the request was made.
        DCHECK_GE(target_tab_id, 0);
        return MirroringActivity::MirroringType::kTab;
      } else {
        NOTREACHED() << "Non-mirroring Cast app: " << source;
        return base::nullopt;
      }
    } else if (source.url().SchemeIsHTTPOrHTTPS()) {
      return MirroringActivity::MirroringType::kOffscreenTab;
    }
  }

  NOTREACHED() << "Invalid source: " << source;
  return base::nullopt;
}

}  // namespace

MirroringActivity::MirroringActivity(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker,
    int target_tab_id,
    const CastSinkExtraData& cast_data,
    OnStopCallback callback)
    : CastActivity(route, app_id, message_handler, session_tracker),
      mirroring_type_(GetMirroringType(route, target_tab_id)),
      cast_data_(cast_data),
      on_stop_(std::move(callback)) {
  if (target_tab_id != -1)
    mirroring_tab_id_ = target_tab_id;
}

MirroringActivity::~MirroringActivity() {
  if (!did_start_mirroring_timestamp_) {
    return;
  }

  auto cast_duration = base::Time::Now() - *did_start_mirroring_timestamp_;
  base::UmaHistogramLongTimes(kHistogramSessionLength, cast_duration);

  if (route().media_source().IsLocalFileSource()) {
    base::UmaHistogramLongTimes(kHistogramSessionLengthFile, cast_duration);
    return;
  }

  if (!mirroring_type_) {
    // The mirroring activity should always be set by now, but check anyway
    // to avoid risk of a segfault.
    return;
  }
  switch (*mirroring_type_) {
    case MirroringType::kTab:
      base::UmaHistogramLongTimes(kHistogramSessionLengthTab, cast_duration);
      break;
    case MirroringType::kDesktop:
      base::UmaHistogramLongTimes(kHistogramSessionLengthScreen, cast_duration);
      break;
    case MirroringType::kOffscreenTab:
      base::UmaHistogramLongTimes(kHistogramSessionLengthOffscreenTab,
                                  cast_duration);
      break;
  }
}

void MirroringActivity::CreateMojoBindings(mojom::MediaRouter* media_router) {
  if (!mirroring_type_)
    return;

  // Get a reference to the mirroring service host.
  switch (*mirroring_type_) {
    case MirroringType::kDesktop: {
      auto stream_id = route_.media_source().DesktopStreamId();
      DCHECK(stream_id);
      media_router->GetMirroringServiceHostForDesktop(
          /* tab_id */ -1, *stream_id, host_.BindNewPipeAndPassReceiver());
      break;
    }
    case MirroringType::kTab:
      DCHECK(mirroring_tab_id_.has_value());
      media_router->GetMirroringServiceHostForTab(
          *mirroring_tab_id_, host_.BindNewPipeAndPassReceiver());
      break;
    case MirroringType::kOffscreenTab:
      media_router->GetMirroringServiceHostForOffscreenTab(
          route_.media_source().url(), route_.presentation_id(),
          host_.BindNewPipeAndPassReceiver());
      break;
  }

  DCHECK(!channel_to_service_receiver_);
  channel_to_service_receiver_ =
      channel_to_service_.BindNewPipeAndPassReceiver();
}

void MirroringActivity::OnError(SessionError error) {
  if (will_start_mirroring_timestamp_) {
    // An error was encountered while attempting to start mirroring.
    base::UmaHistogramEnumeration(kHistogramStartFailureNative, error);
    will_start_mirroring_timestamp_.reset();
  }
  // Metrics for general errors are captured by the mirroring service in
  // MediaRouter.MirroringService.SessionError.
  StopMirroring();
}

void MirroringActivity::DidStart() {
  if (!will_start_mirroring_timestamp_) {
    // DidStart() was called unexpectedly.
    return;
  }
  did_start_mirroring_timestamp_ = base::Time::Now();
  base::UmaHistogramTimes(
      kHistogramSessionLaunch,
      *did_start_mirroring_timestamp_ - *will_start_mirroring_timestamp_);
  DCHECK(mirroring_type_);
  base::UmaHistogramEnumeration(kHistogramStartSuccess, *mirroring_type_);
  will_start_mirroring_timestamp_.reset();
}

void MirroringActivity::DidStop() {
  StopMirroring();
}

void MirroringActivity::Send(mirroring::mojom::CastMessagePtr message) {
  DCHECK(message);
  DVLOG(2) << "Relaying message to receiver: " << message->json_format_data;

  GetDataDecoder().ParseJson(
      message->json_format_data,
      base::BindOnce(&MirroringActivity::HandleParseJsonResult,
                     weak_ptr_factory_.GetWeakPtr(), route().media_route_id()));
}

void MirroringActivity::SendMessageToClient(
    const std::string& client_id,
    blink::mojom::PresentationConnectionMessagePtr message) {
  // A client exists if this is a site-initiated mirroring session. Given client
  // ID is a Cast SDK concept, the client may not have an ID if it joined by
  // directly using the Presentation API, and we wouldn't be able to distinguish
  // them. We also do not expect the mirroring receiver to send any messages, so
  // we drop them.
}

void MirroringActivity::OnAppMessage(
    const cast::channel::CastMessage& message) {
  if (!route_.is_local())
    return;
  if (message.namespace_() != mirroring::mojom::kWebRtcNamespace &&
      message.namespace_() != mirroring::mojom::kRemotingNamespace) {
    // Ignore message with wrong namespace.
    DVLOG(2) << "Ignoring message with namespace " << message.namespace_();
    return;
  }
  DVLOG(2) << "Relaying app message from receiver: " << message.DebugString();
  DCHECK(message.has_payload_utf8());
  DCHECK_EQ(message.protocol_version(),
            cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.namespace_();
  ptr->json_format_data = message.payload_utf8();
  // TODO(jrw): Do something with message.source_id() and
  // message.destination_id()?
  channel_to_service_->Send(std::move(ptr));
}

void MirroringActivity::OnInternalMessage(
    const cast_channel::InternalMessage& message) {
  if (!route_.is_local())
    return;
  DVLOG(2) << "Relaying internal message from receiver: " << message.message;
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.message_namespace;
  CHECK(base::JSONWriter::Write(message.message, &ptr->json_format_data));
  channel_to_service_->Send(std::move(ptr));
}

void MirroringActivity::CreateMediaController(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {}

void MirroringActivity::HandleParseJsonResult(
    const std::string& route_id,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    // TODO(crbug.com/905002): Record UMA metric for parse result.
    DLOG(ERROR) << "Failed to parse Cast client message for " << route_id
                << ": " << *result.error;
    return;
  }

  CastSession* session = GetSession();
  DCHECK(session);

  const std::string message_namespace = GetMirroringNamespace(*result.value);

  cast::channel::CastMessage cast_message = cast_channel::CreateCastMessage(
      message_namespace, std::move(*result.value),
      message_handler_->sender_id(), session->transport_id());
  message_handler_->SendCastMessage(cast_data_.cast_channel_id, cast_message);
}

void MirroringActivity::OnSessionSet(const CastSession& session) {
  if (!mirroring_type_)
    return;

  auto cast_source = CastMediaSource::FromMediaSource(route_.media_source());
  DCHECK(cast_source);

  // Derive session type by intersecting the sink capabilities with what the
  // media source can provide.
  const bool has_audio = (cast_data_.capabilities &
                          static_cast<uint8_t>(cast_channel::AUDIO_OUT)) != 0 &&
                         cast_source->ProvidesStreamingAudioCapture();
  const bool has_video = (cast_data_.capabilities &
                          static_cast<uint8_t>(cast_channel::VIDEO_OUT)) != 0;
  if (!has_audio && !has_video) {
    return;
  }
  const SessionType session_type =
      has_audio && has_video
          ? SessionType::AUDIO_AND_VIDEO
          : has_audio ? SessionType::AUDIO_ONLY : SessionType::VIDEO_ONLY;

  will_start_mirroring_timestamp_ = base::Time::Now();

  // Bind Mojo receivers for the interfaces this object implements.
  mojo::PendingRemote<mirroring::mojom::SessionObserver> observer_remote;
  observer_receiver_.Bind(observer_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mirroring::mojom::CastMessageChannel> channel_remote;
  channel_receiver_.Bind(channel_remote.InitWithNewPipeAndPassReceiver());

  // If this fails, it's probably because CreateMojoBindings() hasn't been
  // called.
  DCHECK(channel_to_service_receiver_);

  host_->Start(SessionParameters::New(
                   session_type, cast_data_.ip_endpoint.address(),
                   cast_data_.model_name, cast_source->target_playout_delay()),
               std::move(observer_remote), std::move(channel_remote),
               std::move(channel_to_service_receiver_));
}

void MirroringActivity::StopMirroring() {
  // Running the callback will cause this object to be deleted.
  if (on_stop_)
    std::move(on_stop_).Run();
}

}  // namespace media_router
