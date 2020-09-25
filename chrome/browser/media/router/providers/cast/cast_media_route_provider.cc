// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"

#include <array>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "url/origin.h"

namespace media_router {

namespace {

constexpr char kLoggerComponent[] = "CastMediaRouteProvider";

// List of origins allowed to use a PresentationRequest to initiate mirroring.
constexpr std::array<base::StringPiece, 2> kPresentationApiAllowlist = {
    "https://docs.google.com",
    "https://meet.google.com",
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
      message_handler_(message_handler) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(media_sink_service_);
  DCHECK(app_discovery_service_);
  DCHECK(message_handler_);

  task_runner->PostTask(
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

  // TODO(crbug.com/816702): This needs to be set properly according to sinks
  // discovered.
  media_router_->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::CAST,
      mojom::MediaRouter::SinkAvailability::PER_SOURCE);
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
                                         int32_t tab_id,
                                         base::TimeDelta timeout,
                                         bool incognito,
                                         CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/809249): Handle mirroring routes, including
  // mirror-to-Cast transitions.
  const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Attempted to create a route with an invalid sink ID",
                      sink_id, source_id, presentation_id);
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Sink not found"),
                            RouteRequestResult::ResultCode::SINK_NOT_FOUND);
    return;
  }

  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromMediaSourceId(source_id);
  if (!cast_source) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Attempted to create a route with an invalid source",
                      sink_id, source_id, presentation_id);
    std::move(callback).Run(
        base::nullopt, nullptr, std::string("Invalid source"),
        RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
    return;
  }

  activity_manager_->LaunchSession(*cast_source, *sink, presentation_id, origin,
                                   tab_id, incognito, std::move(callback));
}

void CastMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t tab_id,
                                       base::TimeDelta timeout,
                                       bool incognito,
                                       JoinRouteCallback callback) {
  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromMediaSourceId(media_source);
  if (!cast_source) {
    std::move(callback).Run(
        base::nullopt, nullptr, std::string("Invalid source"),
        RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Attempted to join a route with an invalid source", "",
                      media_source, presentation_id);
    return;
  }

  if (!activity_manager_) {
    // This should never happen, but it looks like maybe it does.  See
    // crbug.com/1114067.
    NOTREACHED();
    // This message will probably go unnoticed, but it's here to give some
    // indication of what went wrong, since NOTREACHED() is compiled out of
    // release builds.  It would be nice if we could log a message to |logger_|,
    // but it's initialized in the same place as |activity_manager_|, so it's
    // almost certainly not available here.
    LOG(ERROR) << "missing activity manager";
    std::move(callback).Run(base::nullopt, nullptr,
                            "Internal error: missing activity manager",
                            RouteRequestResult::ResultCode::UNKNOWN_ERROR);
    return;
  }

  activity_manager_->JoinSession(*cast_source, presentation_id, origin, tab_id,
                                 incognito, std::move(callback));
}

void CastMediaRouteProvider::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  // TODO(crbug.com/951061): We'll need to implement this to allow joining from
  // the dialog.
  NOTIMPLEMENTED();
  std::move(callback).Run(
      base::nullopt, nullptr, std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
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
  NOTREACHED() << "Binary messages are not supported for Cast routes.";
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

  // A broadcast request is not an actual sink query; it is used to send a
  // app precache message to receivers.
  if (cast_source->broadcast_request()) {
    // TODO(imcheng): Add metric to record broadcast usage.
    BroadcastMessageToSinks(cast_source->GetAppIds(),
                            *cast_source->broadcast_request());
    return;
  }

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

void CastMediaRouteProvider::StartObservingMediaRoutes(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  activity_manager_->AddRouteQuery(media_source);
}

void CastMediaRouteProvider::StopObservingMediaRoutes(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  activity_manager_->RemoveRouteQuery(media_source);
}

void CastMediaRouteProvider::StartListeningForRouteMessages(
    const std::string& route_id) {
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::StopListeningForRouteMessages(
    const std::string& route_id) {
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::DetachRoute(const std::string& route_id) {
  // DetachRoute() isn't implemented. Instead, a presentation connection
  // associated with the route will call DidClose(). See CastSessionClientImpl.
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::EnableMdnsDiscovery() {
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::UpdateMediaSinks(const std::string& media_source) {
  app_discovery_service_->Refresh();
}

void CastMediaRouteProvider::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  NOTIMPLEMENTED();
}

void CastMediaRouteProvider::CreateMediaRouteController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    CreateMediaRouteControllerCallback callback) {
  std::move(callback).Run(activity_manager_->CreateMediaController(
      route_id, std::move(media_controller), std::move(observer)));
}

void CastMediaRouteProvider::GetState(GetStateCallback callback) {
  if (!activity_manager_) {
    std::move(callback).Run(mojom::ProviderState::New());
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
  media_router_->OnSinksReceived(MediaRouteProviderId::CAST, source_id, sinks,
                                 GetOrigins(source_id));
}

void CastMediaRouteProvider::BroadcastMessageToSinks(
    const std::vector<std::string>& app_ids,
    const cast_channel::BroadcastRequest& request) {
  for (const auto& id_and_sink : media_sink_service_->GetSinks()) {
    const MediaSinkInternal& sink = id_and_sink.second;
    message_handler_->SendBroadcastMessage(sink.cast_data().cast_channel_id,
                                           app_ids, request);
  }
}

}  // namespace media_router
