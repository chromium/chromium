// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "chrome/browser/media/cast_mirroring_service_host.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/remoting/device_capability_checker.h"
#include "net/base/url_util.h"
#include "url/origin.h"

namespace media_router {

namespace {

constexpr char kLoggerComponent[] = "CastMediaRouteProvider";

// List of origins allowed to use a PresentationRequest to initiate mirroring.
constexpr std::array<std::string_view, 3> kPresentationApiAllowlist = {
    "https://docs.google.com",
    "https://meet.google.com",
    "https://music.youtube.com",
};

// Returns a list of origins that are valid for |source_id|. An empty list
// means all origins are valid.
// TODO(takumif): Consider returning a nullopt instead of an empty vector to
// indicate all origins.
std::vector<url::Origin> GetOrigins(const MediaSource::Id& source_id) {
  // Use of the mirroring app as a Cast URL is permitted for certain origins as
  // a temporary workaround only. The eventual goal is to support their usecase
  // using generic Presentation API.  See also cast_media_source.cc.
  std::vector<url::Origin> allowed_origins;
  if (IsSiteInitiatedMirroringSource(source_id) &&
      !base::FeatureList::IsEnabled(kAllowAllSitesToInitiateMirroring)) {
    allowed_origins.reserve(kPresentationApiAllowlist.size());
    for (const auto& origin : kPresentationApiAllowlist)
      allowed_origins.push_back(url::Origin::Create(GURL(origin)));
  }
  return allowed_origins;
}

std::optional<media::VideoCodec> ParseVideoCodec(
    const MediaSource& media_source) {
  std::string video_codec;
  if (!net::GetValueForKeyInQuery(media_source.url(), "video_codec",
                                  &video_codec)) {
    return std::nullopt;
  }
  return media::remoting::ParseVideoCodec(video_codec);
}

std::optional<media::AudioCodec> ParseAudioCodec(
    const MediaSource& media_source) {
  std::string audio_codec;
  if (!net::GetValueForKeyInQuery(media_source.url(), "audio_codec",
                                  &audio_codec)) {
    return std::nullopt;
  }
  return media::remoting::ParseAudioCodec(audio_codec);
}

std::vector<MediaSinkInternal> GetRemotePlaybackMediaSourceCompatibleSinks(
    const MediaSource& media_source,
    const std::vector<MediaSinkInternal>& sinks) {
  DCHECK(media_source.IsRemotePlaybackSource());
  std::vector<MediaSinkInternal> compatible_sinks;

  // Return an empty list if the source URL contains invalid codecs. It's
  // possible that the source URL doesn't include an audio codec, which means
  // the media content doesn't have an audio track. However, there must exist a
  // valid video codec.
  auto video_codec = ParseVideoCodec(media_source);
  if (!video_codec.has_value() ||
      video_codec.value() == media::VideoCodec::kUnknown) {
    return compatible_sinks;
  }
  auto audio_codec = ParseAudioCodec(media_source);
  if (audio_codec.has_value() &&
      audio_codec.value() == media::AudioCodec::kUnknown) {
    return compatible_sinks;
  }

  for (const auto& sink : sinks) {
    const std::string& model_name = sink.cast_data().model_name;
    const bool is_supported_model =
        media::remoting::IsKnownToSupportRemoting(model_name);
    const bool is_supported_video_codec =
        media::remoting::IsVideoCodecCompatible(model_name,
                                                video_codec.value());
    const bool is_supported_audio_codec =
        audio_codec.has_value() ? media::remoting::IsAudioCodecCompatible(
                                      model_name, audio_codec.value())
                                : true;

    if (is_supported_model && is_supported_video_codec &&
        is_supported_audio_codec) {
      compatible_sinks.push_back(sink);
    }
  }
  return compatible_sinks;
}

}  // namespace

CastMediaRouteProvider::CastMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router,
    MediaSinkServiceBase* media_sink_service,
    CastAppDiscoveryService* app_discovery_service,
    cast_channel::CastMessageHandler* message_handler,
    const std::string& hash_token,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : media_sink_service_(media_sink_service),
      app_discovery_service_(app_discovery_service),
      message_handler_(message_handler),
      task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(media_sink_service_);
  DCHECK(app_discovery_service_);
  DCHECK(message_handler_);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaRouteProvider::Init, base::Unretained(this),
                     std::move(receiver), std::move(media_router),
                     CastSessionTracker::GetInstance(), hash_token));
}

void CastMediaRouteProvider::Init(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router,
    CastSessionTracker* session_tracker,
    const std::string& hash_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_.Bind(std::move(receiver));
  media_router_.Bind(std::move(media_router));
  media_router_->GetLogger(logger_.BindNewPipeAndPassReceiver());

  activity_manager_ = std::make_unique<CastActivityManager>(
      media_sink_service_, session_tracker, message_handler_,
      media_router_.get(), logger_.get(), hash_token);
}

CastMediaRouteProvider::~CastMediaRouteProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sink_queries_.empty()) {
    DCHECK_EQ(sink_queries_.size(), 1u);
    DCHECK_EQ(sink_queries_.begin()->first, MediaSource::ForAnyTab().id());
  }
}

void CastMediaRouteProvider::CreateRoute(const std::string& source_id,
                                         const std::string& sink_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         int32_t frame_tree_node_id,
                                         base::TimeDelta timeout,
                                         CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40561499): Handle mirroring routes, including
  // mirror-to-Cast transitions.
  const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Attempted to create a route with an invalid sink ID",
                      sink_id, source_id, presentation_id);
    std::move(callback).Run(std::nullopt, nullptr,
                            std::string("Sink not found"),
                            mojom::RouteRequestResultCode::SINK_NOT_FOUND);
    return;
  }

  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromMediaSourceId(source_id);
  if (!cast_source) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Attempted to create a route with an invalid source",
                      sink_id, source_id, presentation_id);
    std::move(callback).Run(
        std::nullopt, nullptr, std::string("Invalid source"),
        mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER);
    return;
  }
  activity_manager_->LaunchSession(*cast_source, *sink, presentation_id, origin,
                                   content::FrameTreeNodeId(frame_tree_node_id),
                                   std::move(callback));
}

void CastMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t frame_tree_node_id,
                                       base::TimeDelta timeout,
                                       JoinRouteCallback callback) {
  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromMediaSourceId(media_source);
  if (!cast_source) {
    std::move(callback).Run(
        std::nullopt, nullptr, std::string("Invalid source"),
        mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER);
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Attempted to join a route with an invalid source", "",
                      media_source, presentation_id);
    return;
  }

  if (!activity_manager_) {
    // This should never happen, but it looks like maybe it does.  See
    // crbug.com/1114067.
    NOTREACHED_IN_MIGRATION();
    // This message will probably go unnoticed, but it's here to give some
    // indication of what went wrong, since NOTREACHED() is compiled out of
    // release builds.  It would be nice if we could log a message to |logger_|,
    // but it's initialized in the same place as |activity_manager_|, so it's
    // almost certainly not available here.
    LOG(ERROR) << "missing activity manager";
    std::move(callback).Run(std::nullopt, nullptr,
                            "Internal error: missing activity manager",
                            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
    return;
  }
  activity_manager_->JoinSession(*cast_source, presentation_id, origin,
                                 content::FrameTreeNodeId(frame_tree_node_id),
                                 std::move(callback));
}

void CastMediaRouteProvider::TerminateRoute(const std::string& route_id,
                                            TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  activity_manager_->TerminateSession(route_id, std::move(callback));
}

void CastMediaRouteProvider::SendRouteMessage(const std::string& media_route_id,
                                              const std::string& message) {
  activity_manager_->SendRouteMessage(media_route_id, message);
}

void CastMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  NOTREACHED_IN_MIGRATION()
      << "Binary messages are not supported for Cast routes.";
}

void CastMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::Contains(sink_queries_, media_source))
    return;

  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromMediaSourceId(media_source);
  if (!cast_source)
    return;

  sink_queries_[media_source] =
      app_discovery_service_->StartObservingMediaSinks(
          *cast_source,
          base::BindRepeating(&CastMediaRouteProvider::OnSinkQueryUpdated,
                              base::Unretained(this)));
}

void CastMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sink_queries_.erase(media_source);
}

void CastMediaRouteProvider::StartObservingMediaRoutes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  activity_manager_->NotifyAllOnRoutesUpdated();
}

void CastMediaRouteProvider::DetachRoute(const std::string& route_id) {
  // DetachRoute() isn't implemented. Instead, a presentation connection
  // associated with the route will call DidClose(). See CastSessionClientImpl.
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::EnableMdnsDiscovery() {
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::DiscoverSinksNow() {
  app_discovery_service_->Refresh();
}

void CastMediaRouteProvider::BindMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    BindMediaControllerCallback callback) {
  std::move(callback).Run(activity_manager_->BindMediaController(
      route_id, std::move(media_controller), std::move(observer)));
}

void CastMediaRouteProvider::GetState(GetStateCallback callback) {
  if (!activity_manager_) {
    std::move(callback).Run(nullptr);
    return;
  }
  const CastSessionTracker::SessionMap& sessions =
      activity_manager_->GetCastSessionTracker()->GetSessions();
  mojom::CastProviderStatePtr cast_state(mojom::CastProviderState::New());
  for (const auto& session : sessions) {
    if (!session.second)
      continue;
    mojom::CastSessionStatePtr session_state(mojom::CastSessionState::New());
    session_state->sink_id = session.first;
    session_state->app_id = session.second->app_id();
    session_state->session_id = session.second->session_id();
    session_state->route_description = session.second->GetRouteDescription();
    cast_state->session_state.emplace_back(std::move(session_state));
  }
  std::move(callback).Run(
      mojom::ProviderState::NewCastProviderState(std::move(cast_state)));
}

void CastMediaRouteProvider::OnSinkQueryUpdated(
    const MediaSource::Id& source_id,
    const std::vector<MediaSinkInternal>& sinks) {
  auto media_source = MediaSource(source_id);
  // Do not check compatibility for non-RemotePlayback MediaSource.
  if (!media_source.IsRemotePlaybackSource()) {
    media_router_->OnSinksReceived(mojom::MediaRouteProviderId::CAST, source_id,
                                   sinks, GetOrigins(source_id));
    return;
  }
  if (!base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen)) {
    return;
  }

  // Check sinks' video/audio compatibility for RemotePlayback MediaSource.
  media_router_->OnSinksReceived(
      mojom::MediaRouteProviderId::CAST, source_id,
      GetRemotePlaybackMediaSourceCompatibleSinks(media_source, sinks),
      GetOrigins(source_id));
}

}  // namespace media_router
