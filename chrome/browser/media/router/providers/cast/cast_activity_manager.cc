// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"
#include "chrome/grit/generated_resources.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "components/media_router/common/route_request_result.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionState;

namespace media_router {

namespace {

constexpr char kLoggerComponent[] = "CastActivityManager";

void RecordSavedDeviceConnectDurationMetric(
    base::Time route_request_creation_timestamp) {
  base::TimeDelta route_success_time =
      base::Time::Now() - route_request_creation_timestamp;
  AccessCodeCastMetrics::RecordSavedDeviceConnectDuration(route_success_time);
}

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

void CastActivityManager::LaunchSession(
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const std::string& presentation_id,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id,
    mojom::MediaRouteProvider::CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cast_source.app_params().empty()) {
    LaunchSessionParsed(cast_source, sink, presentation_id, origin,
                        frame_tree_node_id, std::move(callback),
                        data_decoder::DataDecoder::ValueOrError());
  } else {
    GetDataDecoder().ParseJson(
        cast_source.app_params(),
        base::BindOnce(&CastActivityManager::LaunchSessionParsed,
                       weak_ptr_factory_.GetWeakPtr(), cast_source, sink,
                       presentation_id, origin, frame_tree_node_id,
                       std::move(callback)));
  }
}

void CastActivityManager::LaunchSessionParsed(
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const std::string& presentation_id,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id,
    mojom::MediaRouteProvider::CreateRouteCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!cast_source.app_params().empty() && !result.has_value()) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      base::StrCat({"Error parsing JSON data in appParams: ",
                                    result.error()}),
                      sink.id(), cast_source.source_id(), presentation_id);
    std::move(callback).Run(
        std::nullopt, nullptr, std::string("Invalid JSON Format of appParams"),
        mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER);
    return;
  }

  // If the sink is already associated with a route, then it will be removed
  // when the receiver sends an updated RECEIVER_STATUS message.
  MediaSource source(cast_source.source_id());
  const MediaSink::Id& sink_id = sink.sink().id();
  MediaRoute::Id route_id =
      MediaRoute::GetMediaRouteId(presentation_id, sink_id, source);
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ true);
  route.set_presentation_id(presentation_id);
  route.set_local_presentation(true);
  if (cast_source.ContainsStreamingApp()) {
    route.set_controller_type(RouteControllerType::kMirroring);
  } else {
    route.set_controller_type(RouteControllerType::kGeneric);
  }
  route.set_media_sink_name(sink.sink().name());
  route.set_is_connecting(true);

  // We either have a value, or an error, however `LaunchSession` calls this
  // function is a default constructed `result`, which is supposed to be
  // ignored.
  std::optional<base::Value> opt_result = std::nullopt;
  if (result.has_value() && !result->is_none())
    opt_result = std::move(*result);

  DoLaunchSessionParams params(route, cast_source, sink, origin,
                               frame_tree_node_id, std::move(opt_result),
                               std::move(callback));

  auto activity_it = FindActivityBySink(sink);
  if (activity_it != activities_.end()) {
    // Here we assume that when OnSessionRemoved() is next called for
    // `sink_id`, it will be for removing the pre-existing activity.
    pending_activity_removal_ = {activity_it->second->sink().id(),
                                 activity_it->second->route().media_route_id()};
  }
  DoLaunchSession(std::move(params));
}

void CastActivityManager::DoLaunchSession(DoLaunchSessionParams params) {
  const MediaRoute& route = params.route;
  const MediaRoute::Id& route_id = route.media_route_id();
  const CastMediaSource& cast_source = params.cast_source;
  const MediaSinkInternal& sink = params.sink;
  const content::FrameTreeNodeId frame_tree_node_id = params.frame_tree_node_id;
  std::string app_id = ChooseAppId(cast_source, params.sink);
  auto app_params = std::move(params.app_params);

  if (IsSiteInitiatedMirroringSource(cast_source.source_id())) {
    base::UmaHistogramBoolean(kHistogramAudioSender,
                              cast_source.site_requested_audio_capture());
  }

  cast_source.ContainsStreamingApp()
      ? AddMirroringActivity(route, app_id, frame_tree_node_id,
                             sink.cast_data())
      : AddAppActivity(route, app_id);

  if (frame_tree_node_id) {
    // If there is a route from this frame already, stop it.
    auto route_it = routes_by_frame_.find(frame_tree_node_id);
    if (route_it != routes_by_frame_.end()) {
      TerminateSession(route_it->second, base::DoNothing());
    }
    routes_by_frame_[frame_tree_node_id] = route_id;
  }

  NotifyAllOnRoutesUpdated();
  base::TimeDelta launch_timeout = cast_source.launch_timeout();
  std::vector<std::string> type_str;
  for (ReceiverAppType type : cast_source.supported_app_types()) {
    type_str.emplace_back(cast_util::EnumToString(type).value());
  }
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Sent a Launch Session request.", sink.id(),
                   cast_source.source_id(),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  MaybeShowIssueAtLaunch(MediaSource(cast_source.source_id()), sink.id());
  // `params` gets moved here, and all the variables referencing it cannot be
  // used after that.
  message_handler_->LaunchSession(
      sink.cast_data().cast_channel_id, app_id, launch_timeout, type_str,
      app_params,
      base::BindOnce(&CastActivityManager::HandleLaunchSessionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

AppActivity* CastActivityManager::FindActivityForSessionJoin(
    const CastMediaSource& cast_source,
    const std::string& presentation_id) {
  // We only allow joining by session ID. The Cast SDK uses
  // "cast-session_<Session ID>" as the presentation ID in the reconnect
  // request.
  if (!base::StartsWith(presentation_id, kCastPresentationIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    // TODO(crbug.com/1291725): Find session by presentation_id.
    return nullptr;
  }

  // Find the session ID.
  std::string session_id{
      presentation_id.substr(strlen(kCastPresentationIdPrefix))};

  // Find activity by session ID.  Search should fail if the session ID is not
  // valid.
  auto it = base::ranges::find(
      app_activities_, session_id,
      [](const auto& entry) { return entry.second->session_id(); });
  return it == app_activities_.end() ? nullptr : it->second;
}

AppActivity* CastActivityManager::FindActivityForAutoJoin(
    const CastMediaSource& cast_source,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id) {
  switch (cast_source.auto_join_policy()) {
    case AutoJoinPolicy::kTabAndOriginScoped:
    case AutoJoinPolicy::kOriginScoped:
      break;
    case AutoJoinPolicy::kPageScoped:
      return nullptr;
  }

  auto it = base::ranges::find_if(
      app_activities_,
      [&cast_source, &origin, frame_tree_node_id](const auto& pair) {
        AutoJoinPolicy policy = cast_source.auto_join_policy();
        const AppActivity* activity = pair.second;
        if (!activity->route().is_local())
          return false;
        if (!cast_source.ContainsApp(activity->app_id()))
          return false;
        return activity->HasJoinableClient(policy, origin, frame_tree_node_id);
      });
  return it == app_activities_.end() ? nullptr : it->second;
}

void CastActivityManager::JoinSession(
    const CastMediaSource& cast_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id,
    mojom::MediaRouteProvider::JoinRouteCallback callback) {
  AppActivity* activity = nullptr;
  if (presentation_id == kAutoJoinPresentationId) {
    activity = FindActivityForAutoJoin(cast_source, origin, frame_tree_node_id);
    if (!activity && cast_source.default_action_policy() !=
                         DefaultActionPolicy::kCastThisTab) {
      auto sink = GetSinkForMirroringActivity(frame_tree_node_id);
      if (sink) {
        LaunchSession(cast_source, *sink, presentation_id, origin,
                      frame_tree_node_id, std::move(callback));
        return;
      }
    }
  } else {
    activity = FindActivityForSessionJoin(cast_source, presentation_id);
  }

  if (!activity || !activity->CanJoinSession(cast_source)) {
    std::move(callback).Run(std::nullopt, nullptr,
                            std::string("No matching route"),
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(activity->route().media_sink_id());
  if (!sink) {
    HandleMissingSinkOnJoin(std::move(callback),
                            activity->route().media_sink_id(),
                            cast_source.source_id(), presentation_id);
    return;
  }

  mojom::RoutePresentationConnectionPtr presentation_connection =
      activity->AddClient(cast_source, origin, frame_tree_node_id);

  if (!activity->session_id()) {
    HandleMissingSessionIdOnJoin(std::move(callback));
    return;
  }

  const CastSession* session =
      session_tracker_->GetSessionById(*activity->session_id());
  if (!session) {
    HandleMissingSessionOnJoin(std::move(callback),
                               activity->route().media_sink_id(),
                               cast_source.source_id(), presentation_id);
    return;
  }
  const std::string& client_id = cast_source.client_id();
  activity->SendMessageToClient(
      client_id,
      CreateNewSessionMessage(*session, client_id, *sink, hash_token_));
  message_handler_->EnsureConnection(sink->cast_data().cast_channel_id,
                                     client_id, session->destination_id(),
                                     cast_source.connection_type());

  // Route is now local; update route queries.
  NotifyAllOnRoutesUpdated();
  std::move(callback).Run(activity->route(), std::move(presentation_connection),
                          std::nullopt, mojom::RouteRequestResultCode::OK);
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
  // Keep a copy of route id so it does not get deleted.
  std::string route_id(activity_it->first);
  RemoveActivityWithoutNotification(activity_it, state, close_reason);
  if (state == PresentationConnectionState::CLOSED) {
    media_router_->OnPresentationConnectionClosed(
        route_id, close_reason,
        /* message */ "Activity removed from CastActivityManager.");
  } else {
    media_router_->OnPresentationConnectionStateChanged(route_id, state);
  }
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

  base::EraseIf(routes_by_frame_, [activity_it](const auto& pair) {
    return pair.second == activity_it->first;
  });
  app_activities_.erase(activity_it->first);
  activities_.erase(activity_it);
}

void CastActivityManager::TerminateSession(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string source_id =
      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id);
  const std::string presentation_id =
      MediaRoute::GetPresentationIdFromMediaRouteId(route_id);
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Terminating a session.", "", source_id, presentation_id);
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    logger_->LogWarning(mojom::LogCategory::kRoute, kLoggerComponent,
                        "Cannot find the activity to terminate with route id.",
                        "", source_id, presentation_id);
    std::move(callback).Run("Activity not found",
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  const auto& activity = activity_it->second;
  const auto& session_id = activity->session_id();
  const MediaRoute& route = activity->route();

  // There is no session associated with the route, e.g. the launch request is
  // still pending.
  if (!session_id) {
    // |route_id| might be a reference to the item in |routes_by_frame_|.
    // RemoveActivity() deletes this item in |routes_by_frame_| and invalidates
    // |route_id|.
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Terminated session has no session ID.", "", source_id,
                     presentation_id);
    std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
    return;
  }

  const MediaSinkInternal* sink = media_sink_service_->GetSinkByRoute(route);
  CHECK(sink);

  // TODO(crbug.com/1291748): Get the real client ID.
  std::optional<std::string> client_id = std::nullopt;

  activity->SendStopSessionMessageToClients(hash_token_);
  message_handler_->StopSession(
      sink->cast_channel_id(), *session_id, client_id,
      MakeResultCallbackForRoute(route_id, std::move(callback)));
}

bool CastActivityManager::BindMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    return false;
  }
  activity_it->second->BindMediaController(std::move(media_controller),
                                           std::move(observer));
  return true;
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::FindActivityByChannelId(int channel_id) {
  return base::ranges::find_if(activities_, [channel_id, this](auto& entry) {
    const MediaRoute& route = entry.second->route();
    const MediaSinkInternal* sink = media_sink_service_->GetSinkByRoute(route);
    return sink && sink->cast_data().cast_channel_id == channel_id;
  });
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::FindActivityBySink(const MediaSinkInternal& sink) {
  const MediaSink::Id& sink_id = sink.sink().id();
  return base::ranges::find(activities_, sink_id, [](const auto& activity) {
    return activity.second->route().media_sink_id();
  });
}

MirroringActivity* CastActivityManager::FindMirroringActivityByRouteId(
    const MediaRoute::Id& route_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = activities_.find(route_id);

  // TODO(b/271322325): Add a better API for determining if a route is being
  // used for Cast Streaming. Checking the MediaSource might not be sufficient.
  return it != activities_.end() && it->second->route().controller_type() ==
                                        RouteControllerType::kMirroring
             ? static_cast<MirroringActivity*>(it->second.get())
             : nullptr;
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
    const content::FrameTreeNodeId frame_tree_node_id,
    const CastSinkExtraData& cast_data) {
  // We could theoretically use base::Unretained() below instead of
  // GetWeakPtr(), but that seems like an unnecessary optimization here.
  auto on_stop =
      base::BindOnce(&CastActivityManager::OnActivityStopped,
                     weak_ptr_factory_.GetWeakPtr(), route.media_route_id());
  auto on_source_changed = base::BindRepeating(
      &CastActivityManager::OnSourceChanged, weak_ptr_factory_.GetWeakPtr(),
      route.media_route_id());
  auto activity =
      cast_activity_factory_for_test_
          ? cast_activity_factory_for_test_->MakeMirroringActivity(
                route, app_id, std::move(on_stop), std::move(on_source_changed))
          : std::make_unique<MirroringActivity>(
                route, app_id, message_handler_, session_tracker_,
                frame_tree_node_id, cast_data, std::move(on_stop),
                std::move(on_source_changed));
  activity->CreateMojoBindings(media_router_);
  activity->CreateMirroringServiceHost();
  auto* const activity_ptr = activity.get();
  activities_.emplace(route.media_route_id(), std::move(activity));
  return activity_ptr;
}

void CastActivityManager::OnAppMessage(
    int channel_id,
    const openscreen::cast::proto::CastMessage& message) {
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
    // TODO(crbug.com/40623998): Test this case.
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
  // TODO(crbug.com/1291721): Replace VLOG_IF with an UMA metric.
  VLOG_IF(1, !existing_session_id) << "No existing_session_id.";

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
    // TODO(crbug.com/1291721): Try to come up with a test to exercise this
    // code.  Figure out why this code was originally written to explicitly
    // avoid calling NotifyAllOnRoutesUpdated().
    RemoveActivityWithoutNotification(
        activity_it, PresentationConnectionState::TERMINATED,
        PresentationConnectionCloseReason::CLOSED);
    AddNonLocalActivity(sink, session);
  }
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::OnSessionRemoved(const MediaSinkInternal& sink) {
  auto activity_it = activities_.end();
  if (pending_activity_removal_ &&
      pending_activity_removal_->first == sink.id()) {
    activity_it = activities_.find(pending_activity_removal_->second);
    pending_activity_removal_.reset();
  } else {
    activity_it = FindActivityBySink(sink);
  }
  if (activity_it != activities_.end()) {
    logger_->LogInfo(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Session removed by the receiver.", sink.sink().id(),
        MediaRoute::GetMediaSourceIdFromMediaRouteId(activity_it->first),
        MediaRoute::GetPresentationIdFromMediaRouteId(activity_it->first));
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
  }
}

void CastActivityManager::OnMediaStatusUpdated(
    const MediaSinkInternal& sink,
    const base::Value::Dict& media_status,
    std::optional<int> request_id) {
  auto it = FindActivityBySink(sink);
  if (it != activities_.end()) {
    it->second->SendMediaStatusToClients(media_status, request_id);
  }
}

void CastActivityManager::OnSourceChanged(
    const std::string& media_route_id,
    content::FrameTreeNodeId old_frame_tree_node_id,
    content::FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto current_it = routes_by_frame_.find(old_frame_tree_node_id);
  if (current_it == routes_by_frame_.end() ||
      current_it->second != media_route_id) {
    return;
  }

  auto route_it = routes_by_frame_.find(frame_tree_node_id);
  if (route_it != routes_by_frame_.end()) {
    // Session is terminated as to not allow 2 cast sessions to have the same
    // source tab.
    TerminateSession(route_it->second, base::DoNothing());
  }

  routes_by_frame_.erase(old_frame_tree_node_id);
  routes_by_frame_[frame_tree_node_id] = media_route_id;
}

// This method is only called in one place, so it should probably be inlined.
cast_channel::ResultCallback CastActivityManager::MakeResultCallbackForRoute(
    const std::string& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  return base::BindOnce(&CastActivityManager::HandleStopSessionResponse,
                        weak_ptr_factory_.GetWeakPtr(), route_id,
                        std::move(callback));
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
  const auto get_activity = [&]()
      -> base::expected<std::pair<CastActivity*, std::string>, std::string> {
    if (!result.has_value()) {
      return base::unexpected(
          "Error parsing JSON data when sending route JSON message: " +
          result.error());
    }

    auto* dict = result->GetIfDict();
    if (!dict) {
      return base::unexpected(
          "Error parsing JSON data when sending route JSON message: " +
          result.error());
    }

    const std::string* const client_id = dict->FindString("clientId");
    if (!client_id) {
      return base::unexpected(
          "Cannot send route JSON message without client id.");
    }

    const auto it = activities_.find(media_route_id);
    if (it == activities_.end()) {
      return base::unexpected(
          "No activity found with the given route_id to send route JSON "
          "message.");
    }
    return std::make_pair(it->second.get(), *client_id);
  };
  ASSIGN_OR_RETURN(auto activity, get_activity(), [&](std::string error) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent, std::move(error), "",
        MediaRoute::GetMediaSourceIdFromMediaRouteId(media_route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(media_route_id));
  });
  activity.first->SendMessageToClient(
      std::move(activity.second),
      blink::mojom::PresentationConnectionMessage::NewMessage(message));
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
                   /* is_local */ false);
  route.set_media_sink_name(sink.sink().name());

  CastActivity* activity_ptr = nullptr;
  if (cast_source->ContainsStreamingApp()) {
    route.set_controller_type(RouteControllerType::kMirroring);
    activity_ptr = AddMirroringActivity(
        route, app_id, content::FrameTreeNodeId(), sink.cast_data());
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
  media_router_->OnRoutesUpdated(mojom::MediaRouteProviderId::CAST, routes);
}

void CastActivityManager::HandleLaunchSessionResponse(
    DoLaunchSessionParams params,
    cast_channel::LaunchSessionResponse response,
    cast_channel::LaunchSessionCallbackWrapper* out_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaRoute& route = params.route;
  const MediaRoute::Id& route_id = route.media_route_id();
  const MediaSinkInternal& sink = params.sink;
  const CastMediaSource& cast_source = params.cast_source;

  // Make copies so that they outlive params, which gets moved before they are
  // used.
  const std::string sink_name = sink.sink().name();
  const MediaSink::Id sink_id = sink.sink().id();
  const base::Time request_creation_time = params.creation_time;

  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    const std::string error_message =
        "LaunchSession Response of the route that no longer exists.";
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      error_message, sink.id(), cast_source.source_id(),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    std::move(params.callback)
        .Run(std::nullopt, nullptr, error_message,
             mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  if (response.result != cast_channel::LaunchSessionResponse::Result::kOk) {
    switch (response.result) {
      case cast_channel::LaunchSessionResponse::Result::kPendingUserAuth:
        HandleLaunchSessionResponseMiddleStages(
            std::move(params),
            "Pending user authentication for the cast request", out_callback);
        SendPendingUserAuthNotification(sink_name, sink_id);
        MediaRouterMetrics::RecordMediaRouterPendingUserAuthLatency(
            base::Time::Now() - request_creation_time);
        MediaRouterMetrics::RecordMediaRouterUserPromptWhenLaunchingCast(
            MediaRouterUserPromptWhenLaunchingCast::kPendingUserAuth);
        break;
      case cast_channel::LaunchSessionResponse::Result::kUserAllowed:
        HandleLaunchSessionResponseMiddleStages(
            std::move(params), "The user accepted the cast request",
            out_callback);
        media_router_->ClearTopIssueForSink(sink_id);
        break;
      case cast_channel::LaunchSessionResponse::Result::kUserNotAllowed:
        HandleLaunchSessionResponseFailures(
            activity_it, std::move(params),
            "Failed to launch session as the user declined the cast request.",
            mojom::RouteRequestResultCode::USER_NOT_ALLOWED);
        MediaRouterMetrics::RecordMediaRouterUserPromptWhenLaunchingCast(
            MediaRouterUserPromptWhenLaunchingCast::kUserNotAllowed);
        media_router_->ClearTopIssueForSink(sink_id);
        break;
      case cast_channel::LaunchSessionResponse::Result::kNotificationDisabled:
        HandleLaunchSessionResponseFailures(
            activity_it, std::move(params),
            "Failed to launch session as the notifications are disabled on the "
            "receiver device.",
            mojom::RouteRequestResultCode::NOTIFICATION_DISABLED);
        break;
      case cast_channel::LaunchSessionResponse::Result::kTimedOut:
        HandleLaunchSessionResponseFailures(
            activity_it, std::move(params),
            "Failed to launch session due to timeout.",
            mojom::RouteRequestResultCode::TIMED_OUT);
        break;
      default:
        HandleLaunchSessionResponseFailures(
            activity_it, std::move(params),
            base::StrCat({"Failed to launch session. ", response.error_msg}),
            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
        break;
    }
    return;
  }

  auto session = CastSession::From(sink, *response.receiver_status);
  if (!session) {
    HandleLaunchSessionResponseFailures(
        activity_it, std::move(params),
        "Unable to get session from launch response. Cast session is not "
        "launched.",
        mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }
  RecordLaunchSessionResponseAppType(session->value().Find("appType"));

  mojom::RoutePresentationConnectionPtr presentation_connection;
  const std::string& client_id = cast_source.client_id();
  std::string app_id = ChooseAppId(cast_source, params.sink);
  const auto channel_id = sink.cast_data().cast_channel_id;
  const auto destination_id = session->destination_id();
  auto media_source = MediaSource(cast_source.source_id());

  if (media_source.IsCastPresentationUrl() ||
      media_source.IsRemotePlaybackSource()) {
    presentation_connection = activity_it->second->AddClient(
        cast_source, params.origin, params.frame_tree_node_id);
    if (!client_id.empty()) {
      activity_it->second->SendMessageToClient(
          client_id,
          CreateReceiverActionCastMessage(client_id, sink, hash_token_));
    }
  }

  if (client_id.empty()) {
    if (!cast_source.ContainsStreamingApp()) {
      logger_->LogError(
          mojom::LogCategory::kRoute, kLoggerComponent,
          "The client ID was unexpectedly empty for a non-mirroring app.",
          sink.id(), cast_source.source_id(),
          MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    }
  } else {
    activity_it->second->SendMessageToClient(
        client_id,
        CreateNewSessionMessage(*session, client_id, sink, hash_token_));
  }
  EnsureConnection(client_id, channel_id, destination_id, cast_source);

  activity_it->second->SetOrUpdateSession(*session, sink, hash_token_);

  if (!client_id.empty() && base::Contains(session->message_namespaces(),
                                           cast_channel::kMediaNamespace)) {
    // Request media status from the receiver.
    base::Value::Dict request;
    request.Set("type", cast_util::EnumToString<
                            cast_channel::V2MessageType,
                            cast_channel::V2MessageType::kMediaGetStatus>());
    message_handler_->SendMediaRequest(channel_id, request, client_id,
                                       destination_id);
  }

  activity_it->second->SetRouteIsConnecting(false);
  NotifyAllOnRoutesUpdated();
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Successfully Launched the session.", sink.id(),
                   cast_source.source_id(),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));

  if (sink.cast_data().discovery_type ==
      CastDiscoveryType::kAccessCodeRememberedDevice) {
    RecordSavedDeviceConnectDurationMetric(params.creation_time);
  }

  std::move(params.callback)
      .Run(route, std::move(presentation_connection),
           /* error_text */ std::nullopt, mojom::RouteRequestResultCode::OK);
}

void CastActivityManager::HandleStopSessionResponse(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback,
    cast_channel::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    // The activity could've been removed via RECEIVER_STATUS message.
    std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
    return;
  }

  const std::string source_id =
      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id);
  const std::string presentation_id =
      MediaRoute::GetPresentationIdFromMediaRouteId(route_id);

  if (result == cast_channel::Result::kOk) {
    // |route_id| might be a reference to the item in |routes_by_frame_|.
    // RemoveActivity() deletes this item in |routes_by_frame_| and invalidates
    // |route_id|.
    RemoveActivity(activity_it, PresentationConnectionState::TERMINATED,
                   PresentationConnectionCloseReason::CLOSED);
    std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);

    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Terminated a route successfully after receiving "
                     "StopSession response OK.",
                     "", source_id, presentation_id);
  } else {
    std::string error_msg =
        "StopSession response is not OK. Failed to terminate route.";
    std::move(callback).Run(error_msg,
                            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent, error_msg,
                      "", source_id, presentation_id);
  }
}

void CastActivityManager::HandleLaunchSessionResponseFailures(
    ActivityMap::iterator activity_it,
    DoLaunchSessionParams params,
    const std::string& message,
    mojom::RouteRequestResultCode result_code) {
  logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent, message,
                    params.sink.id(), params.cast_source.source_id(),
                    MediaRoute::GetPresentationIdFromMediaRouteId(
                        params.route.media_route_id()));
  std::move(params.callback).Run(std::nullopt, nullptr, message, result_code);
  RemoveActivity(activity_it, PresentationConnectionState::CLOSED,
                 PresentationConnectionCloseReason::CONNECTION_ERROR);

  if (result_code != mojom::RouteRequestResultCode::USER_NOT_ALLOWED &&
      result_code != mojom::RouteRequestResultCode::NOTIFICATION_DISABLED)
    SendFailedToCastIssue(params.sink.id(), params.route.media_route_id());
}

void CastActivityManager::HandleLaunchSessionResponseMiddleStages(
    DoLaunchSessionParams params,
    const std::string& message,
    cast_channel::LaunchSessionCallbackWrapper* out_callback) {
  DCHECK(out_callback);
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent, message,
                   params.sink.id(), params.cast_source.source_id(),
                   MediaRoute::GetPresentationIdFromMediaRouteId(
                       params.route.media_route_id()));
  out_callback->callback =
      base::BindOnce(&CastActivityManager::HandleLaunchSessionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params));
}

void CastActivityManager::EnsureConnection(const std::string& client_id,
                                           int channel_id,
                                           const std::string& destination_id,
                                           const CastMediaSource& cast_source) {
  // Cast SDK sessions have a |client_id|, and we ensure a virtual connection
  // for them. For mirroring sessions, we ensure a strong virtual connection for
  // |message_handler_|. Mirroring initiated via the Cast SDK will have
  // EnsureConnection() called for both.
  if (!client_id.empty()) {
    message_handler_->EnsureConnection(channel_id, client_id, destination_id,
                                       cast_source.connection_type());
  }
  if (cast_source.ContainsStreamingApp()) {
    message_handler_->EnsureConnection(
        channel_id, message_handler_->source_id(), destination_id,
        cast_channel::VirtualConnectionType::kStrong);
  }
}

void CastActivityManager::SendFailedToCastIssue(
    const MediaSink::Id& sink_id,
    const MediaRoute::Id& route_id) {
  std::string issue_title =
      l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_FAILED_TO_CAST);
  IssueInfo info(issue_title, IssueInfo::Severity::WARNING, sink_id);
  info.route_id = route_id;
  media_router_->OnIssue(info);
}

void CastActivityManager::SendPendingUserAuthNotification(
    const std::string& sink_name,
    const MediaSink::Id& sink_id) {
  std::string issue_title = l10n_util::GetStringFUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_USER_PENDING_AUTHORIZATION,
      base::UTF8ToUTF16(sink_name));

  IssueInfo info(issue_title, IssueInfo::Severity::NOTIFICATION, sink_id);
  media_router_->OnIssue(info);
}

std::optional<MediaSinkInternal>
CastActivityManager::GetSinkForMirroringActivity(
    content::FrameTreeNodeId frame_tree_node_id) const {
  auto route_it = routes_by_frame_.find(frame_tree_node_id);
  if (route_it == routes_by_frame_.end()) {
    return std::nullopt;
  }

  const MediaRoute::Id& route_id = route_it->second;
  if (activities_.find(route_id) != activities_.end() &&
      app_activities_.find(route_id) == app_activities_.end()) {
    return activities_.find(route_id)->second->sink();
  }
  return std::nullopt;
}

std::string CastActivityManager::ChooseAppId(
    const CastMediaSource& source,
    const MediaSinkInternal& sink) const {
  const auto& sink_capabilities = sink.cast_data().capabilities;
  for (const auto& info : source.app_infos()) {
    if (sink_capabilities.HasAll(info.required_capabilities))
      return info.app_id;
  }
  DUMP_WILL_BE_NOTREACHED() << "Can't determine app ID from capabilities.";
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

void CastActivityManager::MaybeShowIssueAtLaunch(
    const MediaSource& media_source,
    const MediaSink::Id& sink_id) {
#if BUILDFLAG(IS_MAC)
  // On macOS, the user cannot choose to share their desktop audio, so we notify
  // the user as such. On other platforms the desktop picker allows the user to
  // manually disable audio capture.
  if (media_source.IsDesktopMirroringSource() &&
      !media_source.IsDesktopSourceWithAudio()) {
    IssueInfo issue_info(
        l10n_util::GetStringUTF8(
            IDS_MEDIA_ROUTER_ISSUE_DESKTOP_AUDIO_NOT_SUPPORTED),
        IssueInfo::Severity::NOTIFICATION, sink_id);
    media_router_->OnIssue(issue_info);
  }
#endif
}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    const MediaRoute& route,
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id,
    const std::optional<base::Value> app_params,
    mojom::MediaRouteProvider::CreateRouteCallback callback)
    : route(route),
      cast_source(cast_source),
      sink(sink),
      origin(origin),
      frame_tree_node_id(frame_tree_node_id),
      creation_time(base::Time::Now()),
      callback(std::move(callback)) {
  if (app_params)
    this->app_params = app_params->Clone();
}

void CastActivityManager::AddMirroringActivityForTest(
    const MediaRoute::Id& route_id,
    std::unique_ptr<MirroringActivity> mirroring_activity) {
  activities_.emplace(route_id, std::move(mirroring_activity));
}

void CastActivityManager::HandleMissingSinkOnJoin(
    mojom::MediaRouteProvider::JoinRouteCallback callback,
    const std::string& sink_id,
    const std::string& source_id,
    const std::string& session_id) {
  logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                    "Cannot find the sink to join with sink_id.", sink_id,
                    source_id, session_id);
  std::move(callback).Run(std::nullopt, nullptr, std::string("Sink not found"),
                          mojom::RouteRequestResultCode::SINK_NOT_FOUND);
}

void CastActivityManager::HandleMissingSessionIdOnJoin(
    mojom::MediaRouteProvider::JoinRouteCallback callback) {
  // This should never happen, but it looks like maybe it does.  See
  // crbug.com/1114067.
  NOTREACHED_IN_MIGRATION();
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
  std::move(callback).Run(std::nullopt, nullptr, kErrorMessage,
                          mojom::RouteRequestResultCode::UNKNOWN_ERROR);
}

void CastActivityManager::HandleMissingSessionOnJoin(
    mojom::MediaRouteProvider::JoinRouteCallback callback,
    const std::string& sink_id,
    const std::string& source_id,
    const std::string& session_id) {
  static const char kErrorMessage[] = "Could not find the session to join.";
  if (logger_) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      kErrorMessage, sink_id, source_id, session_id);
  }
  std::move(callback).Run(std::nullopt, nullptr, kErrorMessage,
                          mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    DoLaunchSessionParams&& other) = default;

CastActivityManager::DoLaunchSessionParams::~DoLaunchSessionParams() = default;

// static
CastActivityFactoryForTest*
    CastActivityManager::cast_activity_factory_for_test_ = nullptr;

}  // namespace media_router
