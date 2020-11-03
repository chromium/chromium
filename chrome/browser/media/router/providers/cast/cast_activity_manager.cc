// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/enum_table.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "url/origin.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionState;

namespace media_router {

namespace {

constexpr char kLoggerComponent[] = "CastActivityManager";

}  // namespace

CastActivityManager::CastActivityManager(
    MediaSinkServiceBase* media_sink_service,
    CastSessionTracker* session_tracker,
    cast_channel::CastMessageHandler* message_handler,
    mojom::MediaRouter* media_router,
    mojom::Logger* logger,
    const std::string& hash_token)
    : media_sink_service_(media_sink_service),
      session_tracker_(session_tracker),
      message_handler_(message_handler),
      media_router_(media_router),
      logger_(logger),
      hash_token_(hash_token) {
  DCHECK(media_sink_service_);
  DCHECK(session_tracker_);
  DCHECK(message_handler_);
  DCHECK(media_router_);
  DCHECK(logger_);
  message_handler_->AddObserver(this);
  for (const auto& sink_id_session : session_tracker_->GetSessions()) {
    const MediaSinkInternal* sink =
        media_sink_service_->GetSinkById(sink_id_session.first);
    if (!sink)
      break;
    AddNonLocalActivity(*sink, *sink_id_session.second);
  }
  session_tracker_->AddObserver(this);
}

CastActivityManager::~CastActivityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This call is needed to ensure mirroring activies are terminated when the
  // browser shuts down.  This works when the browser is closed through its UI,
  // or when it is given an opportunity to shut down gracefully, e.g. with
  // SIGINT on Linux, but not SIGTERM.
  TerminateAllLocalMirroringActivities();

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
    bool off_the_record,
    mojom::MediaRouteProvider::CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cast_source.app_params().empty()) {
    LaunchSessionParsed(cast_source, sink, presentation_id, origin, tab_id,
                        off_the_record, std::move(callback),
                        data_decoder::DataDecoder::ValueOrError());
  } else {
    GetDataDecoder().ParseJson(
        cast_source.app_params(),
        base::BindOnce(&CastActivityManager::LaunchSessionParsed,
                       weak_ptr_factory_.GetWeakPtr(), cast_source, sink,
                       presentation_id, origin, tab_id, off_the_record,
                       std::move(callback)));
  }
}

void CastActivityManager::LaunchSessionParsed(
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    bool off_the_record,
    mojom::MediaRouteProvider::CreateRouteCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!cast_source.app_params().empty() && result.error) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        base::StrCat({"Error parsing JSON data in appParams: ", *result.error}),
        sink.id(), cast_source.source_id(), presentation_id);
    std::move(callback).Run(
        base::nullopt, nullptr, std::string("Invalid JSON Format of appParams"),
        RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
    return;
  }

  // If the sink is already associated with a route, then it will be removed
  // when the receiver sends an updated RECEIVER_STATUS message.
  MediaSource source(cast_source.source_id());
  const MediaSink::Id& sink_id = sink.sink().id();
  MediaRoute::Id route_id =
      MediaRoute::GetMediaRouteId(presentation_id, sink_id, source);
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ true, /* for_display */ true);
  route.set_presentation_id(presentation_id);
  route.set_local_presentation(true);
  route.set_off_the_record(off_the_record);
  if (cast_source.ContainsStreamingApp()) {
    route.set_controller_type(RouteControllerType::kMirroring);
  } else {
    route.set_controller_type(RouteControllerType::kGeneric);
  }
  route.set_media_sink_name(sink.sink().name());

  DoLaunchSessionParams params(route, cast_source, sink, origin, tab_id,
                               std::move(result.value), std::move(callback));

  // If there is currently a session on the sink, it must be terminated before
  // the new session can be launched.
  auto activity_it = std::find_if(
      activities_.begin(), activities_.end(), [&sink_id](const auto& activity) {
        return activity.second->route().media_sink_id() == sink_id;
      });

  if (activity_it == activities_.end()) {
    DoLaunchSession(std::move(params));
  } else {
    const MediaRoute::Id& existing_route_id =
        activity_it->second->route().media_route_id();
    // We cannot launch the new session in the TerminateSession() callback
    // because if we create a session there, then it may get deleted when
    // OnSessionRemoved() is called to notify that the previous session
    // was removed on the receiver.
    TerminateSession(existing_route_id, base::DoNothing());
    // The new session will be launched when OnSessionRemoved() is called for
    // the old session.
    pending_launch_ = std::move(params);
  }
}

void CastActivityManager::DoLaunchSession(DoLaunchSessionParams params) {
  const MediaRoute& route = params.route;
  const CastMediaSource& cast_source = params.cast_source;
  const MediaRoute::Id& route_id = route.media_route_id();
  const MediaSinkInternal& sink = params.sink;
  const int tab_id = params.tab_id;

  if (IsSiteInitiatedMirroringSource(cast_source.source_id())) {
    base::UmaHistogramBoolean(kHistogramAudioSender,
                              cast_source.site_requested_audio_capture());
  }
  RecordLaunchSessionRequestSupportedAppTypes(
      cast_source.supported_app_types());
  std::string app_id = ChooseAppId(cast_source, params.sink);

  mojom::RoutePresentationConnectionPtr presentation_connection;

  CastActivity* activity_ptr =
      cast_source.ContainsStreamingApp()
          ? AddMirroringActivity(route, app_id, tab_id, sink.cast_data())
          : AddAppActivity(route, app_id);
  const std::string& client_id = cast_source.client_id();
  if (MediaSource(cast_source.source_id()).IsCastPresentationUrl()) {
    presentation_connection =
        activity_ptr->AddClient(cast_source, params.origin, tab_id);
    activity_ptr->SendMessageToClient(
        client_id,
        CreateReceiverActionCastMessage(client_id, sink, hash_token_));
  }

  if (tab_id != -1) {
    // If there is a route from this tab already, stop it.
    auto route_it = routes_by_tab_.find(tab_id);
    if (route_it != routes_by_tab_.end()) {
      TerminateSession(route_it->second, base::DoNothing());
    }

    routes_by_tab_[tab_id] = route_id;
  }

  NotifyAllOnRoutesUpdated();
  base::TimeDelta launch_timeout = cast_source.launch_timeout();
  std::vector<std::string> type_str;
  for (ReceiverAppType type : cast_source.supported_app_types()) {
    type_str.push_back(cast_util::EnumToString(type).value().data());
  }
  message_handler_->LaunchSession(
      sink.cast_data().cast_channel_id, app_id, launch_timeout, type_str,
      params.app_params,
      base::BindOnce(&CastActivityManager::HandleLaunchSessionResponse,
                     weak_ptr_factory_.GetWeakPtr(), route_id, sink,
                     cast_source));
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Sent a Launch Session request.", sink.id(),
                   cast_source.source_id(),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));

  std::move(params.callback)
      .Run(route, std::move(presentation_connection),
           /* error_text */ base::nullopt, RouteRequestResult::ResultCode::OK);
}

AppActivity* CastActivityManager::FindActivityForSessionJoin(
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
  auto it = std::find_if(app_activities_.begin(), app_activities_.end(),
                         [&session_id](const auto& entry) {
                           return entry.second->session_id() == session_id;
                         });
  return it == app_activities_.end() ? nullptr : it->second;
}

AppActivity* CastActivityManager::FindActivityForAutoJoin(
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

  auto it =
      std::find_if(app_activities_.begin(), app_activities_.end(),
                   [&cast_source, &origin, tab_id](const auto& pair) {
                     AutoJoinPolicy policy = cast_source.auto_join_policy();
                     const AppActivity* activity = pair.second;
                     if (!activity->route().is_local())
                       return false;
                     if (!cast_source.ContainsApp(activity->app_id()))
                       return false;
                     return activity->HasJoinableClient(policy, origin, tab_id);
                   });
  return it == app_activities_.end() ? nullptr : it->second;
}

void CastActivityManager::JoinSession(
    const CastMediaSource& cast_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    bool off_the_record,
    mojom::MediaRouteProvider::JoinRouteCallback callback) {
  AppActivity* activity = nullptr;
  if (presentation_id == kAutoJoinPresentationId) {
    activity = FindActivityForAutoJoin(cast_source, origin, tab_id);
    if (!activity && cast_source.default_action_policy() !=
                         DefaultActionPolicy::kCastThisTab) {
      auto sink = ConvertMirrorToCast(tab_id);
      if (sink) {
        LaunchSession(cast_source, *sink, presentation_id, origin, tab_id,
                      off_the_record, std::move(callback));
        return;
      }
    }
  } else {
    activity = FindActivityForSessionJoin(cast_source, presentation_id);
  }

  if (!activity || !activity->CanJoinSession(cast_source, off_the_record)) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("No matching route"),
                            RouteRequestResult::ResultCode::ROUTE_NOT_FOUND);
    return;
  }

  // TODO(jrw): Check whether |activity| is from an OffTheRecord route, maybe
  // report INCOGNITO_MISMATCH, or remove INCOGNITO_MISMATCH from
  // RouteRequestResult::ResultCode.  The check is currently performed inside
  // CanJoinSession(), and the behavior is consistent with the old
  // implementation, which never reports an INCOGNITO_MISMATCH error.

  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(activity->route().media_sink_id());
  if (!sink) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Cannot find the sink to join with sink_id.",
                      activity->route().media_sink_id(),
                      cast_source.source_id(), presentation_id);
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Sink not found"),
                            RouteRequestResult::ResultCode::SINK_NOT_FOUND);
    return;
  }

  mojom::RoutePresentationConnectionPtr presentation_connection =
      activity->AddClient(cast_source, origin, tab_id);

  if (!activity->session_id()) {
    // This should never happen, but it looks like maybe it does.  See
    // crbug.com/1114067.
    NOTREACHED();
    static const char kErrorMessage[] = "Internal error: missing session ID";
    // Checking for |logger_| here is pure paranoia, but this code only exists
    // to fix a crash we can't reproduce, so creating even a tiny possibility of
    // a different crash seems like a bad idea.
    if (logger_) {
      // The empty string parameters could have real values, but they're omitted
      // out of an abundance of caution, and they're not especially relevant to
      // this error anyway.
      logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                        kErrorMessage, "", "", "");
    }
    std::move(callback).Run(base::nullopt, nullptr, kErrorMessage,
                            RouteRequestResult::ResultCode::UNKNOWN_ERROR);
    return;
  }

  const CastSession* session =
      session_tracker_->GetSessionById(*activity->session_id());
  const std::string& client_id = cast_source.client_id();
  activity->SendMessageToClient(
      client_id,
      CreateNewSessionMessage(*session, client_id, *sink, hash_token_));
  message_handler_->EnsureConnection(sink->cast_data().cast_channel_id,
                                     client_id, session->transport_id(),
                                     cast_source.connection_type());

  // Route is now local; update route queries.
  NotifyAllOnRoutesUpdated();
  std::move(callback).Run(activity->route(), std::move(presentation_connection),
                          base::nullopt, RouteRequestResult::ResultCode::OK);
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Successfully joined session", sink->id(),
                   cast_source.source_id(), presentation_id);
}

void CastActivityManager::OnActivityStopped(const std::string& route_id) {
  TerminateSession(route_id, base::DoNothing());
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

  base::EraseIf(routes_by_tab_, [activity_it](const auto& pair) {
    return pair.second == activity_it->first;
  });
  app_activities_.erase(activity_it->first);
  activities_.erase(activity_it);
}

void CastActivityManager::TerminateSession(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Terminating a session.", "",
                   MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    logger_->LogWarning(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Cannot find the activity to terminate with route id.", "",
        MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    std::move(callback).Run("Activity not found",
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  const auto& activity = activity_it->second;
  const auto& session_id = activity->session_id();
  const MediaRoute& route = activity->route();

  // There is no session associated with the route, e.g. the launch request is
  // still pending.
  if (!session_id) {
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Terminated session has no session ID.", "",
                     MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                     MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  const MediaSinkInternal* sink = media_sink_service_->GetSinkByRoute(route);
  CHECK(sink);

  // TODO(jrw): Get the real client ID.
  base::Optional<std::string> client_id = base::nullopt;

  activity->SendStopSessionMessageToClients(hash_token_);
  message_handler_->StopSession(
      sink->cast_channel_id(), *session_id, client_id,
      MakeResultCallbackForRoute(route_id, std::move(callback)));
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

AppActivity* CastActivityManager::AddAppActivity(const MediaRoute& route,
                                                 const std::string& app_id) {
  std::unique_ptr<AppActivity> activity(
      cast_activity_factory_for_test_
          ? cast_activity_factory_for_test_->MakeAppActivity(route, app_id)
          : std::make_unique<AppActivity>(route, app_id, message_handler_,
                                          session_tracker_));
  auto* const activity_ptr = activity.get();
  activities_.emplace(route.media_route_id(), std::move(activity));
  app_activities_[route.media_route_id()] = activity_ptr;
  return activity_ptr;
}

CastActivity* CastActivityManager::AddMirroringActivity(
    const MediaRoute& route,
    const std::string& app_id,
    const int tab_id,
    const CastSinkExtraData& cast_data) {
  // NOTE(jrw): We could theoretically use base::Unretained() below instead of
  // GetWeakPtr(), but that seems like an unnecessary optimization here.
  auto on_stop =
      base::BindOnce(&CastActivityManager::OnActivityStopped,
                     weak_ptr_factory_.GetWeakPtr(), route.media_route_id());
  auto activity = cast_activity_factory_for_test_
                      ? cast_activity_factory_for_test_->MakeMirroringActivity(
                            route, app_id, std::move(on_stop))
                      : std::make_unique<MirroringActivity>(
                            route, app_id, message_handler_, session_tracker_,
                            tab_id, cast_data, std::move(on_stop));
  activity->CreateMojoBindings(media_router_);
  auto* const activity_ptr = activity.get();
  activities_.emplace(route.media_route_id(), std::move(activity));
  return activity_ptr;
}

void CastActivityManager::OnAppMessage(
    int channel_id,
    const cast::channel::CastMessage& message) {
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
    AddNonLocalActivity(sink, session);
    NotifyAllOnRoutesUpdated();
    return;
  }

  CastActivity* activity = activity_it->second.get();
  DCHECK(activity->route().media_sink_id() == sink.sink().id());

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
    AddNonLocalActivity(sink, session);
  }
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::OnSessionRemoved(const MediaSinkInternal& sink) {
  auto activity_it = FindActivityBySink(sink);
  if (activity_it != activities_.end()) {
    logger_->LogInfo(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Session removed by the receiver.", sink.sink().id(),
        MediaRoute::GetMediaSourceIdFromMediaRouteId(activity_it->first),
        MediaRoute::GetPresentationIdFromMediaRouteId(activity_it->first));
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
  }
  if (pending_launch_ && pending_launch_->sink.id() == sink.id()) {
    DoLaunchSession(std::move(*pending_launch_));
    pending_launch_.reset();
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

// TODO(jrw): This method is only called in one place.  Just implement the
// functionality there.
cast_channel::ResultCallback CastActivityManager::MakeResultCallbackForRoute(
    const std::string& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  return base::BindOnce(&CastActivityManager::HandleStopSessionResponse,
                        weak_ptr_factory_.GetWeakPtr(), route_id,
                        std::move(callback));
}

const MediaRoute* CastActivityManager::FindMirroringRouteForTab(
    int32_t tab_id) {
  for (const auto& entry : activities_) {
    const std::string& route_id = entry.first;
    const CastActivity& activity = *entry.second;
    if (activity.mirroring_tab_id() == tab_id &&
        !base::Contains(app_activities_, route_id)) {
      return &activity.route();
    }
  }
  return nullptr;
}

void CastActivityManager::SendRouteMessage(const std::string& media_route_id,
                                           const std::string& message) {
  GetDataDecoder().ParseJson(
      message,
      base::BindOnce(&CastActivityManager::SendRouteJsonMessage,
                     weak_ptr_factory_.GetWeakPtr(), media_route_id, message));
}

void CastActivityManager::SendRouteJsonMessage(
    const std::string& media_route_id,
    const std::string& message,
    data_decoder::DataDecoder::ValueOrError result) {
  if (result.error) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Error parsing JSON data when sending route JSON message: " +
            *result.error,
        "", MediaRoute::GetMediaSourceIdFromMediaRouteId(media_route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(media_route_id));
    return;
  }

  const std::string* client_id = result.value->FindStringKey("clientId");
  if (!client_id) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Cannot send route JSON message without client id.", "",
        MediaRoute::GetMediaSourceIdFromMediaRouteId(media_route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(media_route_id));
    return;
  }

  const auto it = activities_.find(media_route_id);
  if (it == activities_.end()) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "No activity found with the given route_id to send route JSON message.",
        "", MediaRoute::GetMediaSourceIdFromMediaRouteId(media_route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(media_route_id));
    return;
  }
  CastActivity& activity = *it->second;

  auto message_ptr =
      blink::mojom::PresentationConnectionMessage::NewMessage(message);
  activity.SendMessageToClient(*client_id, std::move(message_ptr));
}

void CastActivityManager::AddNonLocalActivity(const MediaSinkInternal& sink,
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
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Adding non-local route.", sink_id, cast_source->source_id(),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  // Route description is set in SetOrUpdateSession().
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ false, /* for_display */ true);
  route.set_media_sink_name(sink.sink().name());

  CastActivity* activity_ptr = nullptr;
  if (cast_source->ContainsStreamingApp()) {
    route.set_controller_type(RouteControllerType::kMirroring);
    activity_ptr = AddMirroringActivity(route, app_id, -1, sink.cast_data());
  } else {
    route.set_controller_type(RouteControllerType::kGeneric);
    activity_ptr = AddAppActivity(route, app_id);
  }
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
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "LaunchSession Response of the route that no longer exists.", sink.id(),
        cast_source.source_id(),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    return;
  }

  if (response.result != cast_channel::LaunchSessionResponse::Result::kOk) {
    std::string message;
    switch (response.result) {
      case cast_channel::LaunchSessionResponse::Result::kTimedOut:
        message = "Failed to launch session due to timeout.";
        break;
      default:
        message =
            base::StrCat({"Failed to launch session. ", response.error_msg});
        break;
    }
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent, message,
                      sink.id(), cast_source.source_id(),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    RemoveActivity(activity_it, PresentationConnectionState::CLOSED,
                   PresentationConnectionCloseReason::CONNECTION_ERROR);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  auto session = CastSession::From(sink, *response.receiver_status);
  if (!session) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Unable to get session from launch response. Cast "
                      "session is not launched.",
                      sink.id(), cast_source.source_id(),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    RemoveActivity(activity_it, PresentationConnectionState::CLOSED,
                   PresentationConnectionCloseReason::CONNECTION_ERROR);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }
  RecordLaunchSessionResponseAppType(session->value().FindKey("appType"));

  // Cast SDK sessions have a |client_id|, and we ensure a virtual connection
  // for them. For mirroring sessions, we ensure a strong virtual connection for
  // |message_handler_|. Mirroring initiated via the Cast SDK will have
  // EnsureConnection() called for both.
  const std::string& client_id = cast_source.client_id();
  if (!client_id.empty()) {
    activity_it->second->SendMessageToClient(
        client_id,
        CreateNewSessionMessage(*session, client_id, sink, hash_token_));
    message_handler_->EnsureConnection(sink.cast_data().cast_channel_id,
                                       client_id, session->transport_id(),
                                       cast_source.connection_type());
    // TODO(jrw): Query media status.
  }
  if (cast_source.ContainsStreamingApp()) {
    message_handler_->EnsureConnection(
        sink.cast_data().cast_channel_id, message_handler_->sender_id(),
        session->transport_id(), cast_channel::VirtualConnectionType::kStrong);
  } else if (client_id.empty()) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "The client ID was unexpectedly empty for a non-mirroring app.",
        sink.id(), cast_source.source_id(),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  }

  activity_it->second->SetOrUpdateSession(*session, sink, hash_token_);
  NotifyAllOnRoutesUpdated();
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Successfully Launched the session.", sink.id(),
                   cast_source.source_id(),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
}

void CastActivityManager::HandleStopSessionResponse(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback,
    cast_channel::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Terminated a route successfully after receiving "
                     "StopSession response OK.",
                     "", MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                     MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  } else {
    std::string error_msg =
        "StopSession response is not OK. Failed to terminate route.";
    std::move(callback).Run(error_msg, RouteRequestResult::UNKNOWN_ERROR);
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent, error_msg,
                      "",
                      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
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

std::string CastActivityManager::ChooseAppId(
    const CastMediaSource& source,
    const MediaSinkInternal& sink) const {
  const auto sink_capabilities =
      BitwiseOr<cast_channel::CastDeviceCapability>::FromBits(
          sink.cast_data().capabilities);
  for (const auto& info : source.app_infos()) {
    if (sink_capabilities.HasAll(info.required_capabilities))
      return info.app_id;
  }
  NOTREACHED() << "Can't determine app ID from capabilities.";
  return source.app_infos()[0].app_id;
}

void CastActivityManager::TerminateAllLocalMirroringActivities() {
  // Save all route IDs so we aren't iterating over |activities_| when it's
  // modified.
  std::vector<MediaRoute::Id> route_ids;
  for (const auto& pair : activities_) {
    if (pair.second->route().is_local() &&
        // Anything that isn't an app activity is a mirroring activity.
        app_activities_.find(pair.first) == app_activities_.end()) {
      route_ids.push_back(pair.first);
    }
  }

  // Terminate the activities.
  for (const auto& id : route_ids) {
    TerminateSession(id, base::DoNothing());
  }
}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    const MediaRoute& route,
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const url::Origin& origin,
    int tab_id,
    const base::Optional<base::Value> app_params,
    mojom::MediaRouteProvider::CreateRouteCallback callback)
    : route(route),
      cast_source(cast_source),
      sink(sink),
      origin(origin),
      tab_id(tab_id),
      callback(std::move(callback)) {
  if (app_params)
    this->app_params = app_params->Clone();
}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    DoLaunchSessionParams&& other) = default;

CastActivityManager::DoLaunchSessionParams::~DoLaunchSessionParams() = default;

// static
CastActivityFactoryForTest*
    CastActivityManager::cast_activity_factory_for_test_ = nullptr;

}  // namespace media_router
