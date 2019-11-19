// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_record.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/mirroring_activity_record.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "url/origin.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionState;

namespace media_router {

CastActivityManager::CastActivityManager(
    MediaSinkServiceBase* media_sink_service,
    CastSessionTracker* session_tracker,
    cast_channel::CastMessageHandler* message_handler,
    mojom::MediaRouter* media_router,
    const std::string& hash_token)
    : media_sink_service_(media_sink_service),
      session_tracker_(session_tracker),
      message_handler_(message_handler),
      media_router_(media_router),
      hash_token_(hash_token) {
  DCHECK(media_sink_service_);
  DCHECK(message_handler_);
  DCHECK(media_router_);
  DCHECK(session_tracker_);
  message_handler_->AddObserver(this);
  for (const auto& sink_id_session : session_tracker_->GetSessions()) {
    const MediaSinkInternal* sink =
        media_sink_service_->GetSinkById(sink_id_session.first);
    if (!sink)
      break;
    AddNonLocalActivityRecord(*sink, *sink_id_session.second);
  }
  session_tracker_->AddObserver(this);
}

CastActivityManager::~CastActivityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_handler_->RemoveObserver(this);
  session_tracker_->RemoveObserver(this);
}

void CastActivityManager::AddRouteQuery(const MediaSource::Id& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  route_queries_.insert(source);
  std::vector<MediaRoute> routes = GetRoutes();
  if (!routes.empty())
    NotifyOnRoutesUpdated(source, routes);
}

void CastActivityManager::RemoveRouteQuery(const MediaSource::Id& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  route_queries_.erase(source);
}

void CastActivityManager::LaunchSession(
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    bool incognito,
    mojom::MediaRouteProvider::CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the sink is already associated with a route, then it will be removed
  // when the receiver sends an updated RECEIVER_STATUS message.
  MediaSource source(cast_source.source_id());
  const MediaSink::Id& sink_id = sink.sink().id();
  MediaRoute::Id route_id =
      MediaRoute::GetMediaRouteId(presentation_id, sink_id, source);
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ true, /* for_display */ true);
  route.set_incognito(incognito);
  route.set_controller_type(RouteControllerType::kGeneric);
  DVLOG(1) << "LaunchSession: source_id=" << cast_source.source_id()
           << ", route_id: " << route_id << ", sink_id=" << sink_id;
  DoLaunchSessionParams params(route, cast_source, sink, origin, tab_id,
                               std::move(callback));
  // If there is currently a session on the sink, it must be terminated before
  // the new session can be launched.
  auto it = std::find_if(
      activities_.begin(), activities_.end(), [&sink_id](const auto& activity) {
        return activity.second->route().media_sink_id() == sink_id;
      });
  if (it == activities_.end()) {
    DoLaunchSession(std::move(params));
  } else {
    const MediaRoute::Id& existing_route_id =
        it->second->route().media_route_id();
    TerminateSession(
        existing_route_id,
        base::BindOnce(
            &CastActivityManager::LaunchSessionAfterTerminatingExisting,
            weak_ptr_factory_.GetWeakPtr(), existing_route_id,
            std::move(params)));
  }
}

void CastActivityManager::DoLaunchSession(DoLaunchSessionParams params) {
  const MediaRoute& route = params.route;
  const CastMediaSource& cast_source = params.cast_source;
  const MediaRoute::Id& route_id = route.media_route_id();
  const MediaSinkInternal& sink = params.sink;

  // TODO(crbug.com/904995): In the case of multiple app IDs (e.g. mirroring),
  // we need to choose an appropriate app ID to launch based on capabilities.
  std::string app_id = cast_source.GetAppIds()[0];

  DVLOG(2) << "Launching session with route ID = " << route_id
           << ", source ID = " << cast_source.source_id()
           << ", sink ID = " << sink.sink().id() << ", app ID = " << app_id
           << ", origin = " << params.origin << ", tab ID = " << params.tab_id;

  mojom::RoutePresentationConnectionPtr presentation_connection;

  if (cast_source.ContainsApp(kCastStreamingAppId)) {
    // Lanch a mirroring session.
    AddMirroringActivityRecord(route, app_id, params.tab_id, sink.cast_data());
  } else {
    // Launch a flinging session.
    auto* activity_ptr = AddCastActivityRecord(route, app_id);
    const std::string& client_id = cast_source.client_id();
    if (!client_id.empty()) {
      presentation_connection =
          activity_ptr->AddClient(cast_source, params.origin, params.tab_id);
      activity_ptr->SendMessageToClient(
          client_id,
          CreateReceiverActionCastMessage(client_id, sink, hash_token_));
    }
  }

  NotifyAllOnRoutesUpdated();
  base::TimeDelta launch_timeout = cast_source.launch_timeout();
  message_handler_->LaunchSession(
      sink.cast_data().cast_channel_id, app_id, launch_timeout,
      base::BindOnce(&CastActivityManager::HandleLaunchSessionResponse,
                     weak_ptr_factory_.GetWeakPtr(), route_id, sink,
                     cast_source));

  std::move(params.callback)
      .Run(route, std::move(presentation_connection),
           /* error_text */ base::nullopt, RouteRequestResult::ResultCode::OK);
}

void CastActivityManager::LaunchSessionAfterTerminatingExisting(
    const MediaRoute::Id& existing_route_id,
    DoLaunchSessionParams params,
    const base::Optional<std::string>& error_string,
    RouteRequestResult::ResultCode result) {
  // NOTE(mfoltz): I don't recall if an explicit STOP request is required by
  // Cast V2 before launching a new session.  I think in the Javascript MRP
  // session termination is a fire and forget operation.  In which case we could
  // rely on RECEIVER_STATUS to clean up the state from the just-removed
  // session, versus having to stop then wait for a response.
  DLOG_IF(ERROR, error_string)
      << "Failed to terminate existing session before launching new "
      << "session! New session may not operate correctly. Error: "
      << *error_string;
  activities_.erase(existing_route_id);
  DoLaunchSession(std::move(params));
}

bool CastActivityManager::CanJoinSession(const ActivityRecord& activity,
                                         const CastMediaSource& cast_source,
                                         bool incognito) const {
  if (!cast_source.ContainsApp(activity.app_id()))
    return false;

  if (base::Contains(activity.connected_clients(), cast_source.client_id()))
    return false;

  if (activity.route().is_incognito() != incognito)
    return false;

  return true;
}

ActivityRecord* CastActivityManager::FindActivityForSessionJoin(
    const CastMediaSource& cast_source,
    const std::string& presentation_id) {
  // We only allow joining by session ID. The Cast SDK uses
  // "cast-session_<Session ID>" as the presentation ID in the reconnect
  // request.
  if (!base::StartsWith(presentation_id, kCastPresentationIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    // TODO(jrw): Find session by presentation_id.
    return nullptr;
  }

  // Find the session ID.
  std::string session_id{
      presentation_id.substr(strlen(kCastPresentationIdPrefix))};

  // Find activity by session ID.  Search should fail if the session ID is not
  // valid.
  auto it = std::find_if(activities_.begin(), activities_.end(),
                         [&session_id](const auto& entry) {
                           return entry.second->session_id() == session_id;
                         });
  return it == activities_.end() ? nullptr : it->second.get();
}

ActivityRecord* CastActivityManager::FindActivityForAutoJoin(
    const CastMediaSource& cast_source,
    const url::Origin& origin,
    int tab_id) {
  switch (cast_source.auto_join_policy()) {
    case AutoJoinPolicy::kTabAndOriginScoped:
    case AutoJoinPolicy::kOriginScoped:
      break;
    case AutoJoinPolicy::kPageScoped:
      return nullptr;
  }

  auto it = std::find_if(
      activities_.begin(), activities_.end(),
      [&cast_source, &origin, tab_id](const auto& activity) {
        AutoJoinPolicy policy = cast_source.auto_join_policy();
        const ActivityRecord* record = activity.second.get();
        if (!record->route().is_local())
          return false;
        if (!cast_source.ContainsApp(record->app_id()))
          return false;
        const auto& clients = record->connected_clients();
        return std::any_of(clients.begin(), clients.end(),
                           [policy, &origin, tab_id](const auto& client) {
                             return IsAutoJoinAllowed(policy, origin, tab_id,
                                                      client.second->origin(),
                                                      client.second->tab_id());
                           });
      });
  return it == activities_.end() ? nullptr : it->second.get();
}

void CastActivityManager::JoinSession(
    const CastMediaSource& cast_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    bool incognito,
    mojom::MediaRouteProvider::JoinRouteCallback callback) {
  DVLOG(2) << "JoinSession: source / presentation ID: "
           << cast_source.source_id() << ", " << presentation_id;
  ActivityRecord* activity = nullptr;
  if (presentation_id == kAutoJoinPresentationId) {
    activity = FindActivityForAutoJoin(cast_source, origin, tab_id);
    if (!activity && cast_source.default_action_policy() !=
                         DefaultActionPolicy::kCastThisTab) {
      auto sink = ConvertMirrorToCast(tab_id);
      if (sink) {
        LaunchSession(cast_source, *sink, presentation_id, origin, tab_id,
                      incognito, std::move(callback));
        return;
      }
    }
  } else {
    activity = FindActivityForSessionJoin(cast_source, presentation_id);
  }

  if (!activity || !CanJoinSession(*activity, cast_source, incognito)) {
    DVLOG(2) << "No joinable activity";
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("No matching route"),
                            RouteRequestResult::ResultCode::ROUTE_NOT_FOUND);
    return;
  }
  DVLOG(2) << "Found activity to join: " << activity->route().media_route_id();

  // TODO(jrw): Check whether |activity| is from an incognito route, maybe
  // report INCOGNITO_MISMATCH, or remove INCOGNITO_MISMATCH from
  // RouteRequestResult::ResultCode.  The check is currently performed inside
  // CanJoinSession(), and the behavior is consistent with the old
  // implementation, which never reports an INCOGNITO_MISMATCH error.

  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(activity->route().media_sink_id());
  if (!sink) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Sink not found"),
                            RouteRequestResult::ResultCode::SINK_NOT_FOUND);
    return;
  }

  mojom::RoutePresentationConnectionPtr presentation_connection =
      activity->AddClient(cast_source, origin, tab_id);

  DCHECK(activity->session_id());
  const CastSession* session =
      session_tracker_->GetSessionById(*activity->session_id());
  const std::string& client_id = cast_source.client_id();
  activity->SendMessageToClient(
      client_id,
      CreateNewSessionMessage(*session, client_id, *sink, hash_token_));
  message_handler_->EnsureConnection(sink->cast_data().cast_channel_id,
                                     client_id, session->transport_id());

  // Route is now local; update route queries.
  NotifyAllOnRoutesUpdated();
  std::move(callback).Run(activity->route(), std::move(presentation_connection),
                          base::nullopt, RouteRequestResult::ResultCode::OK);
}

// TODO(jrw): Can this be merged with HandleStopSessionResponse?
void CastActivityManager::RemoveActivityByRouteId(const std::string& route_id) {
  auto it = activities_.find(route_id);
  if (it != activities_.end()) {
    RemoveActivity(it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
  }
}

void CastActivityManager::RemoveActivity(
    ActivityMap::iterator activity_it,
    PresentationConnectionState state,
    PresentationConnectionCloseReason close_reason) {
  RemoveActivityWithoutNotification(activity_it, state, close_reason);
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::RemoveActivityWithoutNotification(
    ActivityMap::iterator activity_it,
    PresentationConnectionState state,
    PresentationConnectionCloseReason close_reason) {
  switch (state) {
    case PresentationConnectionState::CLOSED:
      activity_it->second->ClosePresentationConnections(close_reason);
      break;
    case PresentationConnectionState::TERMINATED:
      activity_it->second->TerminatePresentationConnections();
      break;
    default:
      DLOG(ERROR) << "Invalid state: " << state;
  }

  activities_.erase(activity_it);
}

void CastActivityManager::TerminateSession(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    std::move(callback).Run("Activity not found",
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  DVLOG(2) << "Terminating session with route ID: " << route_id;

  const auto& activity = activity_it->second;
  const auto& session_id = activity->session_id();
  const MediaRoute& route = activity->route();

  // There is no session associated with the route, e.g. the launch request is
  // still pending.
  if (!session_id) {
    DLOG(WARNING) << "Terminated route has no session ID.";
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  const MediaSinkInternal* sink = media_sink_service_->GetSinkByRoute(route);
  CHECK(sink);

  activity->SendStopSessionMessageToReceiver(
      base::nullopt,  // TODO(jrw): Get the real client ID.
      hash_token_, std::move(callback));
}

bool CastActivityManager::CreateMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end())
    return false;
  activity_it->second->CreateMediaController(std::move(media_controller),
                                             std::move(observer));
  return true;
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::FindActivityByChannelId(int channel_id) {
  return std::find_if(
      activities_.begin(), activities_.end(), [channel_id, this](auto& entry) {
        const MediaRoute& route = entry.second->route();
        const MediaSinkInternal* sink =
            media_sink_service_->GetSinkByRoute(route);
        return sink && sink->cast_data().cast_channel_id == channel_id;
      });
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::FindActivityBySink(const MediaSinkInternal& sink) {
  const MediaSink::Id& sink_id = sink.sink().id();
  return std::find_if(
      activities_.begin(), activities_.end(), [&sink_id](const auto& activity) {
        return activity.second->route().media_sink_id() == sink_id;
      });
}

ActivityRecord* CastActivityManager::AddCastActivityRecord(
    const MediaRoute& route,
    const std::string& app_id) {
  std::unique_ptr<ActivityRecord> activity;
  if (activity_record_factory_) {
    activity = activity_record_factory_->MakeCastActivityRecord(route, app_id);
  } else {
    activity.reset(new CastActivityRecord(route, app_id, media_sink_service_,
                                          message_handler_, session_tracker_,
                                          this));
  }
  auto* activity_ptr = activity.get();
  activities_.emplace(route.media_route_id(), std::move(activity));
  return activity_ptr;
}

ActivityRecord* CastActivityManager::AddMirroringActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    int tab_id,
    const CastSinkExtraData& cast_data) {
  auto activity = std::make_unique<MirroringActivityRecord>(
      route, app_id, message_handler_, session_tracker_, tab_id, cast_data,
      media_router_,
      // We could theoretically use base::Unretained() below instead of
      // GetWeakPtr(), the that seems like an unnecessary optimization here.
      // --jrw
      base::BindOnce(&CastActivityManager::RemoveActivityByRouteId,
                     weak_ptr_factory_.GetWeakPtr(), route.media_route_id()));
  auto* activity_ptr = activity.get();
  activities_.emplace(route.media_route_id(), std::move(activity));
  return activity_ptr;
}

void CastActivityManager::OnAppMessage(
    int channel_id,
    const cast_channel::CastMessage& message) {
  // Note: app messages are received only after session is created.
  DVLOG(2) << "Received app message on cast channel " << channel_id;
  auto it = FindActivityByChannelId(channel_id);
  if (it == activities_.end()) {
    DVLOG(2) << "No activity associated with channel!";
    return;
  }
  it->second->OnAppMessage(message);
}

void CastActivityManager::OnInternalMessage(
    int channel_id,
    const cast_channel::InternalMessage& message) {
  DVLOG(2) << "Received internal message on cast channel " << channel_id;
  auto it = FindActivityByChannelId(channel_id);
  if (it == activities_.end()) {
    DVLOG(2) << "No activity associated with channel!";
    return;
  }
  it->second->OnInternalMessage(message);
}

void CastActivityManager::OnSessionAddedOrUpdated(const MediaSinkInternal& sink,
                                                  const CastSession& session) {
  auto activity_it = FindActivityByChannelId(sink.cast_data().cast_channel_id);

  // If |activity| is null, we have discovered a non-local activity.
  if (activity_it == activities_.end()) {
    // TODO(crbug.com/954797): Test this case.
    AddNonLocalActivityRecord(sink, session);
    NotifyAllOnRoutesUpdated();
    return;
  }

  ActivityRecord* activity = activity_it->second.get();
  DCHECK(activity->route().media_sink_id() == sink.sink().id());

  DVLOG(2) << "Receiver status: update/replace activity: "
           << activity->route().media_route_id();
  const auto& existing_session_id = activity->session_id();

  // This condition seems to always be true in practice, but if it's not, we
  // still try to handle them gracefully below.
  //
  // TODO(jrw): Replace DCHECK with an UMA metric.
  DCHECK(existing_session_id);

  // If |existing_session_id| is empty, then most likely it's due to a pending
  // launch. Check the app ID to see if the existing activity should be
  // updated or replaced.  Otherwise, check the session ID to see if the
  // existing activity should be updated or replaced.
  if (existing_session_id ? existing_session_id == session.session_id()
                          : activity->app_id() == session.app_id()) {
    activity->SetOrUpdateSession(session, sink, hash_token_);
  } else {
    // NOTE(jrw): This happens if a receiver switches to a new session (or
    // app), causing the activity associated with the old session to be
    // considered remote.  This scenario is tested in the unit tests, but it's
    // unclear whether it even happens in practice; I haven't been able to
    // trigger it.
    //
    // TODO(jrw): Try to come up with a test to exercise this code.
    //
    // TODO(jrw): Figure out why this code was originally written to
    // explicitly avoid calling NotifyAllOnRoutesUpdated().
    RemoveActivityWithoutNotification(
        activity_it, PresentationConnectionState::TERMINATED,
        PresentationConnectionCloseReason::CLOSED);
    AddNonLocalActivityRecord(sink, session);
  }
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::OnSessionRemoved(const MediaSinkInternal& sink) {
  auto it = FindActivityBySink(sink);
  if (it != activities_.end()) {
    RemoveActivity(it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
  }
}

void CastActivityManager::OnMediaStatusUpdated(const MediaSinkInternal& sink,
                                               const base::Value& media_status,
                                               base::Optional<int> request_id) {
  auto it = FindActivityBySink(sink);
  if (it != activities_.end()) {
    it->second->SendMediaStatusToClients(media_status, request_id);
  }
}

cast_channel::ResultCallback CastActivityManager::MakeResultCallbackForRoute(
    const std::string& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  return base::BindOnce(&CastActivityManager::HandleStopSessionResponse,
                        weak_ptr_factory_.GetWeakPtr(), route_id,
                        std::move(callback));
}

void CastActivityManager::AddNonLocalActivityRecord(
    const MediaSinkInternal& sink,
    const CastSession& session) {
  const MediaSink::Id& sink_id = sink.sink().id();

  // We derive the MediaSource from a session using the app ID.
  const std::string& app_id = session.app_id();
  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromAppId(app_id);
  MediaSource source(cast_source->source_id());

  // The session ID is used instead of presentation ID in determining the
  // route ID.
  MediaRoute::Id route_id =
      MediaRoute::GetMediaRouteId(session.session_id(), sink_id, source);
  DVLOG(2) << "Adding non-local route " << route_id
           << " with sink ID = " << sink_id
           << ", session ID = " << session.session_id()
           << ", app ID = " << app_id;
  // Route description is set in SetOrUpdateSession().
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ false, /* for_display */ true);

  auto* activity_ptr = AddCastActivityRecord(route, app_id);
  activity_ptr->SetOrUpdateSession(session, sink, hash_token_);
}

const MediaRoute* CastActivityManager::GetRoute(
    const MediaRoute::Id& route_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = activities_.find(route_id);
  return it != activities_.end() ? &(it->second->route()) : nullptr;
}

std::vector<MediaRoute> CastActivityManager::GetRoutes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<MediaRoute> routes;
  for (const auto& activity : activities_)
    routes.push_back(activity.second->route());

  return routes;
}

void CastActivityManager::NotifyAllOnRoutesUpdated() {
  std::vector<MediaRoute> routes = GetRoutes();
  for (const auto& source_id : route_queries_)
    NotifyOnRoutesUpdated(source_id, routes);
}

void CastActivityManager::NotifyOnRoutesUpdated(
    const MediaSource::Id& source_id,
    const std::vector<MediaRoute>& routes) {
  // Note: joinable_route_ids is empty as we are deprecating the join feature
  // in the Harmony UI.
  media_router_->OnRoutesUpdated(MediaRouteProviderId::CAST, routes, source_id,
                                 std::vector<MediaRoute::Id>());
}

void CastActivityManager::HandleLaunchSessionResponse(
    const MediaRoute::Id& route_id,
    const MediaSinkInternal& sink,
    const CastMediaSource& cast_source,
    cast_channel::LaunchSessionResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    DVLOG(2) << "Route no longer exists";
    return;
  }

  if (response.result != cast_channel::LaunchSessionResponse::Result::kOk) {
    DLOG(ERROR) << "Failed to launch session for " << route_id;
    RemoveActivity(activity_it, PresentationConnectionState::CLOSED,
                   PresentationConnectionCloseReason::CONNECTION_ERROR);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  auto session = CastSession::From(sink, *response.receiver_status);
  if (!session) {
    DLOG(ERROR) << "Unable to get session from launch response";
    RemoveActivity(activity_it, PresentationConnectionState::CLOSED,
                   PresentationConnectionCloseReason::CONNECTION_ERROR);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  const std::string& client_id = cast_source.client_id();
  if (!client_id.empty()) {
    DVLOG(2) << "Sending new_session message for route " << route_id
             << ", client_id: " << client_id;
    activity_it->second->SendMessageToClient(
        client_id,
        CreateNewSessionMessage(*session, client_id, sink, hash_token_));

    // TODO(jrw): Query media status.
    message_handler_->EnsureConnection(sink.cast_data().cast_channel_id,
                                       client_id, session->transport_id());
  }

  activity_it->second->SetOrUpdateSession(*session, sink, hash_token_);
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::HandleStopSessionResponse(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback,
    cast_channel::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__ << ": " << route_id;

  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    // The activity could've been removed via RECEIVER_STATUS message.
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  if (result == cast_channel::Result::kOk) {
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
  } else {
    std::move(callback).Run("Failed to terminate route",
                            RouteRequestResult::UNKNOWN_ERROR);
  }
}

void CastActivityManager::SendFailedToCastIssue(
    const MediaSink::Id& sink_id,
    const MediaRoute::Id& route_id) {
  // TODO(crbug.com/989237): i18n-ize the title string.
  IssueInfo info("Failed to cast. Please try again.",
                 IssueInfo::Action::DISMISS, IssueInfo::Severity::WARNING);
  info.sink_id = sink_id;
  info.route_id = route_id;
  media_router_->OnIssue(info);
}

base::Optional<MediaSinkInternal> CastActivityManager::ConvertMirrorToCast(
    int tab_id) {
  for (const auto& pair : activities_) {
    if (pair.second->mirroring_tab_id() == tab_id) {
      return pair.second->sink();
    }
  }

  return base::nullopt;
}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    const MediaRoute& route,
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const url::Origin& origin,
    int tab_id,
    mojom::MediaRouteProvider::CreateRouteCallback callback)
    : route(route),
      cast_source(cast_source),
      sink(sink),
      origin(origin),
      tab_id(tab_id),
      callback(std::move(callback)) {}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    DoLaunchSessionParams&& other) = default;

CastActivityManager::DoLaunchSessionParams::~DoLaunchSessionParams() = default;

CastActivityRecordFactoryForTest*
    CastActivityManager::activity_record_factory_ = nullptr;

}  // namespace media_router
