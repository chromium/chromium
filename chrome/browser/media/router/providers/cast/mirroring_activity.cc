// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "components/media_router/common/route_request_result.h"
#include "components/mirroring/mojom/session_parameters.mojom-forward.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"
#include "ui/base/l10n/l10n_util.h"

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
constexpr char kHistogramSessionLengthAccessCode[] =
    "MediaRouter.CastStreaming.Session.Length.AccessCode";
constexpr char kHistogramSessionLengthOffscreenTab[] =
    "MediaRouter.CastStreaming.Session.Length.OffscreenTab";
constexpr char kHistogramSessionLengthScreen[] =
    "MediaRouter.CastStreaming.Session.Length.Screen";
constexpr char kHistogramSessionLengthTab[] =
    "MediaRouter.CastStreaming.Session.Length.Tab";
constexpr char kHistogramStartFailureAccessCodeManualEntry[] =
    "MediaRouter.CastStreaming.Start.Failure.AccessCodeManualEntry";
constexpr char kHistogramStartFailureAccessCodeRememberedDevice[] =
    "MediaRouter.CastStreaming.Start.Failure.AccessCodeRememberedDevice";
constexpr char kHistogramStartFailureNative[] =
    "MediaRouter.CastStreaming.Start.Failure.Native";
constexpr char kHistogramStartSuccess[] =
    "MediaRouter.CastStreaming.Start.Success";
constexpr char kHistogramStartSuccessAccessCodeManualEntry[] =
    "MediaRouter.CastStreaming.Start.Success.AccessCodeManualEntry";
constexpr char kHistogramStartSuccessAccessCodeRememberedDevice[] =
    "MediaRouter.CastStreaming.Start.Success.AccessCodeRememberedDevice";

constexpr char kLoggerComponent[] = "MirroringService";

using MirroringType = MirroringActivity::MirroringType;

const std::string GetMirroringNamespace(const base::Value::Dict& message) {
  const std::string* type = message.FindString("type");
  if (type &&
      *type == cast_util::EnumToString<cast_channel::CastMessageType,
                                       cast_channel::CastMessageType::kRpc>()) {
    return mirroring::mojom::kRemotingNamespace;
  } else {
    return mirroring::mojom::kWebRtcNamespace;
  }
}

absl::optional<MirroringActivity::MirroringType> GetMirroringType(
    const MediaRoute& route) {
  if (!route.is_local())
    return absl::nullopt;

  const auto source = route.media_source();
  if (source.IsTabMirroringSource())
    return MirroringActivity::MirroringType::kTab;
  if (source.IsDesktopMirroringSource())
    return MirroringActivity::MirroringType::kDesktop;

  if (base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen) &&
      source.IsRemotePlaybackSource()) {
    return MirroringActivity::MirroringType::kTab;
  }

  if (!source.url().is_valid()) {
    NOTREACHED() << "Invalid source: " << source;
    return absl::nullopt;
  }

  if (source.IsCastPresentationUrl()) {
    const auto cast_source = CastMediaSource::FromMediaSource(source);
    if (cast_source && cast_source->ContainsStreamingApp()) {
      // Site-initiated Mirroring has a Cast Presentation URL and contains
      // StreamingApp. We should return Tab Mirroring here.
      return MirroringActivity::MirroringType::kTab;
    } else {
      NOTREACHED() << "Non-mirroring Cast app: " << source;
      return absl::nullopt;
    }
  } else if (source.url().SchemeIsHTTPOrHTTPS()) {
    return MirroringActivity::MirroringType::kOffscreenTab;
  }

  NOTREACHED() << "Invalid source: " << source;
  return absl::nullopt;
}

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
bool ShouldForceLetterboxing(base::StringPiece model_name) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "disable-cast-letterboxing")) {
    return false;
  }
  return model_name.find("Nest Hub") != base::StringPiece::npos;
}

void AutoSwitchToFlingingIfNeeded(const std::string& sink_id,
                                  int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;

  base::WeakPtr<WebContentsPresentationManager>
      web_contents_presentation_manager =
          WebContentsPresentationManager::Get(web_contents);
  if (!web_contents_presentation_manager ||
      !web_contents_presentation_manager->HasDefaultPresentationRequest()) {
    return;
  }

  auto* media_router = MediaRouterFactory::GetApiForBrowserContextIfExists(
      web_contents->GetBrowserContext());
  if (!media_router)
    return;

  const auto& presentation_request =
      web_contents_presentation_manager->GetDefaultPresentationRequest();
  const auto source_id =
      MediaSource::ForPresentationUrl(presentation_request.presentation_urls[0])
          .id();
  bool incognito = web_contents->GetBrowserContext()->IsOffTheRecord();
  media_router->JoinRoute(
      source_id, kAutoJoinPresentationId, presentation_request.frame_origin,
      web_contents,
      base::BindOnce(&WebContentsPresentationManager::OnPresentationResponse,
                     std::move(web_contents_presentation_manager),
                     presentation_request),
      base::TimeDelta(), incognito);
}

}  // namespace

MirroringActivity::MirroringActivity(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker,
    int frame_tree_node_id,
    const CastSinkExtraData& cast_data,
    OnStopCallback callback)
    : CastActivity(route, app_id, message_handler, session_tracker),
      mirroring_type_(GetMirroringType(route)),
      frame_tree_node_id_(frame_tree_node_id),
      cast_data_(cast_data),
      on_stop_(std::move(callback)) {}

MirroringActivity::~MirroringActivity() {
  if (!did_start_mirroring_timestamp_) {
    return;
  }

  auto cast_duration = base::Time::Now() - *did_start_mirroring_timestamp_;
  base::UmaHistogramLongTimes(kHistogramSessionLength, cast_duration);

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
  CastDiscoveryType discovery_type = cast_data_.discovery_type;
  if (discovery_type == CastDiscoveryType::kAccessCodeManualEntry ||
      discovery_type == CastDiscoveryType::kAccessCodeRememberedDevice) {
    base::UmaHistogramLongTimes(kHistogramSessionLengthAccessCode,
                                cast_duration);
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
          *stream_id, host_.BindNewPipeAndPassReceiver());
      break;
    }
    case MirroringType::kTab:
      media_router->GetMirroringServiceHostForTab(
          frame_tree_node_id_, host_.BindNewPipeAndPassReceiver());
      break;
    case MirroringType::kOffscreenTab:
      media_router->GetMirroringServiceHostForOffscreenTab(
          route_.media_source().url(), route_.presentation_id(),
          host_.BindNewPipeAndPassReceiver());
      break;
  }
  media_router->GetLogger(logger_.BindNewPipeAndPassReceiver());

  DCHECK(!channel_to_service_receiver_);
  channel_to_service_receiver_ =
      channel_to_service_.BindNewPipeAndPassReceiver();
}

void MirroringActivity::OnError(SessionError error) {
  logger_->LogError(
      media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
      base::StringPrintf(
          "Mirroring will stop. MirroringService.SessionError: %d",
          static_cast<int>(error)),
      route_.media_sink_id(), route_.media_source().id(),
      route_.presentation_id());
  if (will_start_mirroring_timestamp_) {
    // An error was encountered while attempting to start mirroring.
    base::UmaHistogramEnumeration(kHistogramStartFailureNative, error);

    // Record the error for access code discovery types.
    CastDiscoveryType discovery_type = cast_data_.discovery_type;
    if (discovery_type == CastDiscoveryType::kAccessCodeManualEntry) {
      base::UmaHistogramEnumeration(
          kHistogramStartFailureAccessCodeManualEntry, error);
    } else if (discovery_type ==
               CastDiscoveryType::kAccessCodeRememberedDevice) {
      base::UmaHistogramEnumeration(
          kHistogramStartFailureAccessCodeRememberedDevice, error);
    }

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

  // Record successes to access code discovery types.
  CastDiscoveryType discovery_type = cast_data_.discovery_type;
  if (discovery_type == CastDiscoveryType::kAccessCodeManualEntry) {
    base::UmaHistogramEnumeration(kHistogramStartSuccessAccessCodeManualEntry,
                                  *mirroring_type_);
  } else if (discovery_type ==
             CastDiscoveryType::kAccessCodeRememberedDevice) {
    base::UmaHistogramEnumeration(
        kHistogramStartSuccessAccessCodeRememberedDevice, *mirroring_type_);
  }

  will_start_mirroring_timestamp_.reset();
}

void MirroringActivity::DidStop() {
  StopMirroring();
}

void MirroringActivity::LogInfoMessage(const std::string& message) {
  logger_->LogInfo(media_router::mojom::LogCategory::kMirroring,
                   kLoggerComponent, message, route_.media_sink_id(),
                   route_.media_source().id(), route_.presentation_id());
}

void MirroringActivity::LogErrorMessage(const std::string& message) {
  logger_->LogError(media_router::mojom::LogCategory::kMirroring,
                    kLoggerComponent, message, route_.media_sink_id(),
                    route_.media_source().id(), route_.presentation_id());
}

void MirroringActivity::OnSourceChanged() {
  host_->GetTabSourceId(base::BindOnce(&MirroringActivity::UpdateSourceTab,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void MirroringActivity::UpdateSourceTab(int32_t frame_tree_node_id) {
  if (frame_tree_node_id == -1 || frame_tree_node_id == frame_tree_node_id_)
    return;

  session_tracker_->OnSourceChanged(route_.media_route_id(),
                                    frame_tree_node_id_, frame_tree_node_id);
  frame_tree_node_id_ = frame_tree_node_id;

  // Posting to UI thread, as while obtaining WebContents instance through
  // `FromFrameTreeNodeId()`, a call to `GloballyFindByID()` would happen and
  // that is only allowed on UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AutoSwitchToFlingingIfNeeded,
                                route_.media_sink_id(), frame_tree_node_id_));
}

void MirroringActivity::OnMessage(mirroring::mojom::CastMessagePtr message) {
  DCHECK(message);
  DVLOG(2) << "Relaying message to receiver: " << message->json_format_data;

  GetDataDecoder().ParseJson(
      message->json_format_data,
      base::BindOnce(&MirroringActivity::HandleParseJsonResult,
                     weak_ptr_factory_.GetWeakPtr(), route().media_route_id()));
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
  CastSession* session = GetSession();
  if (!session) {
    DVLOG(2) << "No valid session.";
    return;
  }

  if (message.destination_id() != session->destination_id() &&
      message.destination_id() != "*") {
    // Ignore messages sent to someone else.
    DVLOG(2) << "Ignoring message intended for destination_id:\""
             << message.destination_id() << "\" (expected \""
             << session->destination_id() << "\").";
    return;
  }

  if (message.source_id() != message_handler_->source_id()) {
    // Ignore messages sent by a stranger.
    DVLOG(2) << "Ignoring message unexpectedly sent by source_id: \""
             << message.source_id() << "\" (expected \""
             << message_handler_->source_id() << "\")";
    return;
  }

  DVLOG(2) << "Relaying app message from receiver: " << message.DebugString();
  DCHECK(message.has_payload_utf8());
  DCHECK_EQ(message.protocol_version(),
            cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  if (message.namespace_() == mirroring::mojom::kWebRtcNamespace) {
    logger_->LogInfo(media_router::mojom::LogCategory::kMirroring,
                     kLoggerComponent,
                     base::StrCat({"Relaying app message from receiver:",
                                   message.payload_utf8()}),
                     route().media_sink_id(), route().media_source().id(),
                     route().presentation_id());
  }

  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.namespace_();
  ptr->json_format_data = message.payload_utf8();
  channel_to_service_->OnMessage(std::move(ptr));
}

void MirroringActivity::OnInternalMessage(
    const cast_channel::InternalMessage& message) {
  if (!route_.is_local())
    return;
  DVLOG(2) << "Relaying internal message from receiver: " << message.message;
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.message_namespace;
  CHECK(base::JSONWriter::Write(message.message, &ptr->json_format_data));
  if (message.message_namespace == mirroring::mojom::kWebRtcNamespace) {
    logger_->LogInfo(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StrCat({"Relaying internal WebRTC message from receiver: ",
                      ptr->json_format_data}),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
  }
  channel_to_service_->OnMessage(std::move(ptr));
}

void MirroringActivity::CreateMediaController(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {}

std::string MirroringActivity::GetRouteDescription(
    const CastSession& session) const {
  if (!mirroring_type_) {
    return CastActivity::GetRouteDescription(session);
  }
  switch (*mirroring_type_) {
    case MirroringActivity::MirroringType::kTab:
      return l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_CASTING_TAB);
    case MirroringActivity::MirroringType::kDesktop:
      return l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_CASTING_DESKTOP);
    case MirroringActivity::MirroringType::kOffscreenTab:
      return l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_PRESENTATION_ROUTE_DESCRIPTION,
          base::UTF8ToUTF16(route().media_source().url().host()));
  }
}

void MirroringActivity::HandleParseJsonResult(
    const std::string& route_id,
    data_decoder::DataDecoder::ValueOrError result) {
  CastSession* session = GetSession();
  DCHECK(session);

  if (!result.has_value() || !result.value().is_dict()) {
    // TODO(crbug.com/905002): Record UMA metric for parse result.
    logger_->LogError(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StrCat({"Failed to parse Cast client message:", result.error()}),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
    return;
  }

  const std::string message_namespace =
      GetMirroringNamespace(result.value().GetDict());
  if (message_namespace == mirroring::mojom::kWebRtcNamespace) {
    logger_->LogInfo(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StrCat({"WebRTC message received: ",
                      GetScrubbedLogMessage(result.value().GetDict())}),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
  }

  cast::channel::CastMessage cast_message = cast_channel::CreateCastMessage(
      message_namespace, std::move(*result), message_handler_->source_id(),
      session->destination_id());
  if (message_handler_->SendCastMessage(cast_data_.cast_channel_id,
                                        cast_message) == Result::kFailed) {
    logger_->LogError(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StringPrintf(
            "Failed to send Cast message to channel_id: %d, in namespace: %s",
            cast_data_.cast_channel_id, message_namespace.c_str()),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
  }
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

  host_->Start(
      SessionParameters::New(
          session_type, cast_data_.ip_endpoint.address(), cast_data_.model_name,
          sink_.sink().name(), session.destination_id(),
          message_handler_->source_id(), cast_source->target_playout_delay(),
          route().media_source().IsRemotePlaybackSource(),
          ShouldForceLetterboxing(cast_data_.model_name)),
      std::move(observer_remote), std::move(channel_remote),
      std::move(channel_to_service_receiver_), route_.media_sink_name());
}

void MirroringActivity::StopMirroring() {
  // Running the callback will cause this object to be deleted.
  if (on_stop_)
    std::move(on_stop_).Run();
}

std::string MirroringActivity::GetScrubbedLogMessage(
    const base::Value::Dict& message) {
  std::string message_str;
  auto scrubbed_message = message.Clone();
  base::Value::List* streams =
      scrubbed_message.FindListByDottedPath("offer.supportedStreams");
  if (!streams) {
    base::JSONWriter::Write(scrubbed_message, &message_str);
    return message_str;
  }

  for (base::Value& item : *streams) {
    if (!item.is_dict()) {
      continue;
    }
    if (item.GetDict().FindString("aesKey")) {
      item.GetDict().Set("aesKey", "AES_KEY");
    }
    if (item.GetDict().FindString("aesIvMask")) {
      item.GetDict().Set("aesIvMask", "AES_IV_MASK");
    }
  }
  base::JSONWriter::Write(scrubbed_message, &message_str);
  return message_str;
}

}  // namespace media_router
