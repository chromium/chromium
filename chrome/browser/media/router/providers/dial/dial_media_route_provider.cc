// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/dial/dial_media_route_provider_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/media_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace media_router {

namespace {
constexpr char kLoggerComponent[] = "DialMediaRouteProvider";

url::Origin CreateOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

static constexpr int kMaxPendingDialLaunches = 10;

}  // namespace

DialMediaRouteProvider::DialMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router,
    DialMediaSinkServiceImpl* media_sink_service,
    const std::string& hash_token,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : media_sink_service_(media_sink_service),
      internal_message_util_(hash_token) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(media_sink_service_);

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DialMediaRouteProvider::Init, base::Unretained(this),
                     std::move(receiver), std::move(media_router)));
}

void DialMediaRouteProvider::Init(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_.Bind(std::move(receiver));
  media_router_.Bind(std::move(media_router));
  media_sink_service_->AddObserver(this);

  media_router_->GetLogger(logger_.BindNewPipeAndPassReceiver());

  // |activity_manager_| might have already been set in tests.
  if (!activity_manager_)
    activity_manager_ = std::make_unique<DialActivityManager>(
        media_sink_service_->app_discovery_service());
}

DialMediaRouteProvider::~DialMediaRouteProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_sink_queries_.empty());
  media_sink_service_->RemoveObserver(this);
}

void DialMediaRouteProvider::CreateRoute(const std::string& media_source,
                                         const std::string& sink_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         int32_t frame_tree_node_id,
                                         base::TimeDelta timeout,
                                         CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to create route. Cannot find sink with the sink id", sink_id,
        media_source, presentation_id);
    std::move(callback).Run(std::nullopt, nullptr, "Unknown sink " + sink_id,
                            mojom::RouteRequestResultCode::SINK_NOT_FOUND);
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kSinkNotFound);
    return;
  }

  auto activity =
      DialActivity::From(presentation_id, *sink, media_source, origin);
  if (!activity) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Failed to create route. Unsupported source.", sink_id,
                      media_source, presentation_id);
    std::move(callback).Run(
        std::nullopt, nullptr, "Unsupported source " + media_source,
        mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER);
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kUnsupportedSource);
    return;
  }

  const MediaRoute::Id& route_id = activity->route.media_route_id();
  if (activity_manager_->GetActivity(route_id) != nullptr) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Failed to create route. Route already exists.", sink_id,
                      media_source, presentation_id);
    std::move(callback).Run(
        std::nullopt, nullptr, "Route already exists",
        mojom::RouteRequestResultCode::ROUTE_ALREADY_EXISTS);
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kRouteAlreadyExists);
    return;
  }
  // Check if there's already a route to sink.
  if (activity_manager_->GetActivityBySinkId(sink_id) != nullptr) {
    // Terminate the existing session before creating new one.
    TerminateRoute(
        activity_manager_->GetActivityBySinkId(sink_id)->route.media_route_id(),
        base::DoNothing());
    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Existing route terminated successfully.", sink_id,
                      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  }

  activity_manager_->AddActivity(*activity);
  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Successfully created a new route.", sink_id, media_source,
                   presentation_id);
  std::move(callback).Run(activity->route, /*presentation_connection*/ nullptr,
                          /*error_text*/ std::nullopt,
                          mojom::RouteRequestResultCode::OK);

  // When a custom DIAL launch request is received, DialMediaRouteProvider will
  // create a MediaRoute immediately in order to start exchanging messages with
  // the Cast SDK to complete the launch sequence. The first messages that the
  // MRP needs to send are the RECEIVER_ACTION and NEW_SESSION.
  std::vector<mojom::RouteMessagePtr> messages;
  messages.emplace_back(internal_message_util_.CreateReceiverActionCastMessage(
      activity->launch_info.client_id, *sink));
  messages.emplace_back(internal_message_util_.CreateNewSessionMessage(
      activity->launch_info.app_name, activity->launch_info.client_id, *sink));
  media_router_->OnRouteMessagesReceived(route_id, std::move(messages));
}

void DialMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t frame_tree_node_id,
                                       base::TimeDelta timeout,
                                       JoinRouteCallback callback) {
  const DialActivity* activity = activity_manager_->GetActivityToJoin(
      presentation_id, MediaSource(media_source), origin);
  if (activity) {
    std::move(callback).Run(
        activity->route, /*presentation_connection*/ nullptr,
        /*error_text*/ std::nullopt, mojom::RouteRequestResultCode::OK);
  } else {
    std::move(callback).Run(std::nullopt, /*presentation_connection*/ nullptr,
                            "DIAL activity not found",
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
  }
}

void DialMediaRouteProvider::TerminateRoute(const std::string& route_id,
                                            TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const DialActivity* activity = activity_manager_->GetActivity(route_id);
  if (!activity) {
    logger_->LogInfo(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to terminate route. Route not found with route id.", "",
        MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    std::move(callback).Run("Activity not found",
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  const MediaRoute& route = activity->route;
  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(route.media_sink_id());
  if (!sink) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Failed to terminate route. Sink not found with sink id.",
                      route.media_sink_id(),
                      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    std::move(callback).Run("Sink not found",
                            mojom::RouteRequestResultCode::SINK_NOT_FOUND);
    return;
  }

  DoTerminateRoute(*activity, *sink, std::move(callback));
}

void DialMediaRouteProvider::SendRouteMessage(const std::string& media_route_id,
                                              const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetDataDecoder().ParseJson(
      message, base::BindOnce(&DialMediaRouteProvider::HandleParsedRouteMessage,
                              weak_ptr_factory_.GetWeakPtr(), media_route_id));
}

void DialMediaRouteProvider::HandleParsedRouteMessage(
    const MediaRoute::Id& route_id,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result.value().is_dict()) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        base::StrCat({"Failed to parse the route message. ", result.error()}),
        "", MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    return;
  }

  std::string error;
  std::unique_ptr<DialInternalMessage> internal_message =
      DialInternalMessage::From(std::move(result.value().GetDict()), &error);
  if (!internal_message) {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      base::StrCat({"Invalid route message. ", error}), "",
                      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    return;
  }

  const DialActivity* activity = activity_manager_->GetActivity(route_id);
  if (!activity) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to handle the route message. Route not found with route id.",
        "", MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    return;
  }

  const MediaRoute& route = activity->route;
  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(route.media_sink_id());
  if (!sink) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to handle the route message. Sink not found with sink id.",
        route.media_sink_id(),
        MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    return;
  }

  // TODO(crbug.com/40090609): Investigate whether the direct use of
  // PresentationConnection in this class to communicate with the SDK client can
  // result in eliminating the need for CLIENT_CONNECT messages.
  if (internal_message->type == DialInternalMessageType::kClientConnect) {
    HandleClientConnect(*activity, *sink);
  } else if (internal_message->type ==
             DialInternalMessageType::kCustomDialLaunch) {
    HandleCustomDialLaunchResponse(*activity, *internal_message);
  } else if (internal_message->type == DialInternalMessageType::kDialAppInfo) {
    HandleDiapAppInfoRequest(*activity, *internal_message, *sink);
  } else if (DialInternalMessageUtil::IsStopSessionMessage(*internal_message)) {
    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Received a stop session message.", route.media_sink_id(),
                     MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                     MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    DoTerminateRoute(*activity, *sink, base::DoNothing());
  }
}

void DialMediaRouteProvider::HandleClientConnect(
    const DialActivity& activity,
    const MediaSinkInternal& sink) {
  // Get the latest app info required to handle a CUSTOM_DIAL_LAUNCH message to
  // the SDK client.
  media_sink_service_->app_discovery_service()->FetchDialAppInfo(
      sink, activity.launch_info.app_name,
      base::BindOnce(&DialMediaRouteProvider::SendCustomDialLaunchMessage,
                     weak_ptr_factory_.GetWeakPtr(),
                     activity.route.media_route_id()));
}

void DialMediaRouteProvider::SendCustomDialLaunchMessage(
    const MediaRoute::Id& route_id,
    const MediaSink::Id& sink_id,
    const std::string& app_name,
    DialAppInfoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if there is app info.
  if (!result.app_info) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to send custom app launch message. Cannot find app info.",
        sink_id, MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    // Note: this leaves the route in a stuck state; the client must terminate
    // the route. Maybe we should clean up the route here.
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kAppInfoNotFound);
    return;
  }

  // Check if activity still exists.
  auto* activity = activity_manager_->GetActivity(route_id);
  if (!activity) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to send custom app launch message. The route is closed.",
        sink_id, MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    return;
  }

  auto* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        "Failed to send custom app launch message. Sink not found.", sink_id,
        MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    // TODO(imcheng): We should remove the route when the sink is removed.
    return;
  }

  logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                   "Sending custom app launch message", sink->id(),
                   MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));

  auto message_and_seq_number =
      internal_message_util_.CreateCustomDialLaunchMessage(
          activity->launch_info.client_id, *sink, *result.app_info);
  pending_dial_launches_.insert(message_and_seq_number.second);
  if (pending_dial_launches_.size() > kMaxPendingDialLaunches) {
    pending_dial_launches_.erase(pending_dial_launches_.begin());
  }

  std::vector<mojom::RouteMessagePtr> messages;
  messages.emplace_back(std::move(message_and_seq_number.first));
  media_router_->OnRouteMessagesReceived(route_id, std::move(messages));
}

void DialMediaRouteProvider::SendDialAppInfoResponse(
    const MediaRoute::Id& route_id,
    int sequence_number,
    const MediaSink::Id& sink_id,
    const std::string& app_name,
    DialAppInfoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* activity = activity_manager_->GetActivity(route_id);
  auto* sink = media_sink_service_->GetSinkById(sink_id);
  // If the activity no longer exists, there is no need to inform the sender
  // client of the activity status.
  if (!activity || !sink) {
    return;
  }
  mojom::RouteMessagePtr message;
  if (result.app_info) {
    message = internal_message_util_.CreateDialAppInfoMessage(
        activity->launch_info.client_id, *sink, *result.app_info,
        sequence_number, DialInternalMessageType::kDialAppInfo);
  } else {
    message = internal_message_util_.CreateDialAppInfoErrorMessage(
        result.result_code, activity->launch_info.client_id, sequence_number,
        result.error_message, result.http_error_code);
  }
  std::vector<mojom::RouteMessagePtr> messages;
  messages.emplace_back(std::move(message));
  media_router_->OnRouteMessagesReceived(route_id, std::move(messages));
}

void DialMediaRouteProvider::HandleCustomDialLaunchResponse(
    const DialActivity& activity,
    const DialInternalMessage& message) {
  if (!pending_dial_launches_.erase(message.sequence_number)) {
    return;
  }

  const MediaRoute::Id& media_route_id = activity.route.media_route_id();
  activity_manager_->LaunchApp(
      media_route_id, CustomDialLaunchMessageBody::From(message),
      base::BindOnce(&DialMediaRouteProvider::HandleAppLaunchResult,
                     base::Unretained(this), media_route_id));
}

void DialMediaRouteProvider::HandleDiapAppInfoRequest(
    const DialActivity& activity,
    const DialInternalMessage& message,
    const MediaSinkInternal& sink) {
  media_sink_service_->app_discovery_service()->FetchDialAppInfo(
      sink, activity.launch_info.app_name,
      base::BindOnce(&DialMediaRouteProvider::SendDialAppInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     activity.route.media_route_id(), message.sequence_number));
}

void DialMediaRouteProvider::HandleAppLaunchResult(
    const MediaRoute::Id& route_id,
    bool success) {
  if (success) {
    logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                     "Successfully launched app.",
                     MediaRoute::GetSinkIdFromMediaRouteId(route_id),
                     MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                     MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kSuccess);
  } else {
    logger_->LogError(mojom::LogCategory::kRoute, kLoggerComponent,
                      "Failed to launch app.",
                      MediaRoute::GetSinkIdFromMediaRouteId(route_id),
                      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                      MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kAppLaunchFailed);
  }
  NotifyAllOnRoutesUpdated();
}

void DialMediaRouteProvider::DoTerminateRoute(const DialActivity& activity,
                                              const MediaSinkInternal& sink,
                                              TerminateRouteCallback callback) {
  const MediaRoute::Id& route_id = activity.route.media_route_id();
  std::pair<std::optional<std::string>, mojom::RouteRequestResultCode>
      can_stop_app = activity_manager_->CanStopApp(route_id);
  if (can_stop_app.second == mojom::RouteRequestResultCode::OK) {
    std::vector<mojom::RouteMessagePtr> messages;
    messages.emplace_back(
        internal_message_util_.CreateReceiverActionStopMessage(
            activity.launch_info.client_id, sink));
    media_router_->OnRouteMessagesReceived(route_id, std::move(messages));
    activity_manager_->StopApp(
        route_id,
        base::BindOnce(&DialMediaRouteProvider::HandleStopAppResult,
                       base::Unretained(this), route_id, std::move(callback)));
  } else {
    logger_->LogError(
        mojom::LogCategory::kRoute, kLoggerComponent,
        base::StringPrintf(
            "Failed to terminate route. %s mojom::RouteRequestResultCode: %d",
            can_stop_app.first.value_or("").c_str(),
            static_cast<int>(can_stop_app.second)),
        sink.id(), MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
        MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
    std::move(callback).Run(can_stop_app.first, can_stop_app.second);
  }
}

void DialMediaRouteProvider::HandleStopAppResult(
    const MediaRoute::Id& route_id,
    TerminateRouteCallback callback,
    const std::optional<std::string>& message,
    mojom::RouteRequestResultCode result_code) {
  switch (result_code) {
    case mojom::RouteRequestResultCode::OK:
      logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                       "Successfully terminated route.", "",
                       MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                       MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
      break;
    case mojom::RouteRequestResultCode::ROUTE_ALREADY_TERMINATED:
      logger_->LogInfo(mojom::LogCategory::kRoute, kLoggerComponent,
                       "Tried to stop a session that no longer exists.", "",
                       MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                       MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
      break;
    default:
      // In this case, the app may still be running on the receiver but we can
      // not terminate it. So, we remove it from the list of routes tracked by
      // Chrome, and inform the user to stop it from the receiver side as well.
      logger_->LogError(
          mojom::LogCategory::kRoute, kLoggerComponent,
          base::StringPrintf("Removed a route that may still be running on the "
                             "receiver. %s RouteRequestResult: %d",
                             message.value_or("").c_str(),
                             static_cast<int>(result_code)),
          "", MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
          MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
      media_router_->OnIssue(
          {l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_CANNOT_TERMINATE),
           IssueInfo::Severity::WARNING,
           MediaRoute::GetSinkIdFromMediaRouteId(route_id)});
  }
  // We set the PresentationConnection state to "terminated" per the API spec:
  // https://w3c.github.io/presentation-api/#terminating-a-presentation-in-a-controlling-browsing-context
  media_router_->OnPresentationConnectionStateChanged(
      route_id, blink::mojom::PresentationConnectionState::TERMINATED);
  NotifyAllOnRoutesUpdated();
  std::move(callback).Run(message, result_code);
}

void DialMediaRouteProvider::NotifyAllOnRoutesUpdated() {
  auto routes = activity_manager_->GetRoutes();
  NotifyOnRoutesUpdated(routes);
}

void DialMediaRouteProvider::NotifyOnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  media_router_->OnRoutesUpdated(mojom::MediaRouteProviderId::DIAL, routes);
}

void DialMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (media_source.empty()) {
    std::vector<MediaSinkInternal> sinks;
    for (const auto& sink_it : media_sink_service_->GetSinks())
      sinks.push_back(sink_it.second);
    OnSinksDiscovered(sinks);
    return;
  }

  MediaSource dial_source(media_source);
  if (!dial_source.IsDialSource())
    return;

  std::string app_name = dial_source.AppNameFromDialSource();
  if (app_name.empty())
    return;

  auto& sink_query = media_sink_queries_[app_name];
  if (!sink_query) {
    sink_query = std::make_unique<MediaSinkQuery>();
    sink_query->subscription =
        media_sink_service_->StartMonitoringAvailableSinksForApp(
            app_name, base::BindRepeating(
                          &DialMediaRouteProvider::OnAvailableSinksUpdated,
                          base::Unretained(this)));
  }

  // Return cached results immediately.
  if (sink_query->media_sources.insert(dial_source).second) {
    auto sinks = media_sink_service_->GetAvailableSinks(app_name);
    NotifyOnSinksReceived(dial_source.id(), sinks, GetOrigins(app_name));
  }
}

void DialMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MediaSource dial_source(media_source);
  std::string app_name = dial_source.AppNameFromDialSource();
  if (!dial_source.id().empty() && app_name.empty())
    return;

  const auto& sink_query_it = media_sink_queries_.find(app_name);
  if (sink_query_it == media_sink_queries_.end())
    return;

  auto& media_sources = sink_query_it->second->media_sources;
  media_sources.erase(dial_source);
  if (media_sources.empty())
    media_sink_queries_.erase(sink_query_it);
}

void DialMediaRouteProvider::StartObservingMediaRoutes() {
  // Return current set of routes.
  auto routes = activity_manager_->GetRoutes();
  if (!routes.empty())
    NotifyOnRoutesUpdated(routes);
}

void DialMediaRouteProvider::DetachRoute(const std::string& route_id) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::EnableMdnsDiscovery() {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::DiscoverSinksNow() {
  media_sink_service_->DiscoverSinksNow();
}

void DialMediaRouteProvider::BindMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    BindMediaControllerCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void DialMediaRouteProvider::GetState(GetStateCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(mojom::ProviderStatePtr());
}

void DialMediaRouteProvider::SetActivityManagerForTest(
    std::unique_ptr<DialActivityManager> activity_manager) {
  DCHECK(!activity_manager_);
  activity_manager_ = std::move(activity_manager);
}

void DialMediaRouteProvider::OnSinksDiscovered(
    const std::vector<MediaSinkInternal>& sinks) {
  // Send a list of all available sinks to Media Router as sinks not associated
  // with any particular source.
  NotifyOnSinksReceived(MediaSource::Id(), sinks, {});
}

void DialMediaRouteProvider::OnAvailableSinksUpdated(
    const std::string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& sink_query_it = media_sink_queries_.find(app_name);
  if (sink_query_it == media_sink_queries_.end()) {
    return;
  }

  const auto& media_sources = sink_query_it->second->media_sources;
  std::vector<url::Origin> origins = GetOrigins(app_name);

  auto sinks = media_sink_service_->GetAvailableSinks(app_name);
  for (const auto& media_source : media_sources)
    NotifyOnSinksReceived(media_source.id(), sinks, origins);
}

void DialMediaRouteProvider::NotifyOnSinksReceived(
    const MediaSource::Id& source_id,
    const std::vector<MediaSinkInternal>& sinks,
    const std::vector<url::Origin>& origins) {
  media_router_->OnSinksReceived(mojom::MediaRouteProviderId::DIAL, source_id,
                                 sinks, origins);
}

std::vector<url::Origin> DialMediaRouteProvider::GetOrigins(
    const std::string& app_name) {
  static const base::NoDestructor<
      base::flat_map<std::string, std::vector<url::Origin>>>
      origin_allowlist(
          {{"YouTube",
            {CreateOrigin("https://music.youtube.com/"),
             CreateOrigin("https://music-green-qa.youtube.com/"),
             CreateOrigin("https://music-release-qa.youtube.com/"),
             CreateOrigin("https://tv.youtube.com"),
             CreateOrigin("https://tv-green-qa.youtube.com"),
             CreateOrigin("https://tv-release-qa.youtube.com"),
             CreateOrigin("https://web-green-qa.youtube.com"),
             CreateOrigin("https://web-release-qa.youtube.com"),
             CreateOrigin("https://www.youtube.com")}},
           {"Netflix", {CreateOrigin("https://www.netflix.com")}},
           {"Pandora", {CreateOrigin("https://www.pandora.com")}},
           {"Radio", {CreateOrigin("https://www.pandora.com")}},
           {"Hulu", {CreateOrigin("https://www.hulu.com")}},
           {"Vimeo", {CreateOrigin("https://www.vimeo.com")}},
           {"Dailymotion", {CreateOrigin("https://www.dailymotion.com")}},
           {"com.dailymotion", {CreateOrigin("https://www.dailymotion.com")}}});

  auto origins_it = origin_allowlist->find(app_name);
  if (origins_it == origin_allowlist->end())
    return std::vector<url::Origin>();

  return origins_it->second;
}

DialMediaRouteProvider::MediaSinkQuery::MediaSinkQuery() = default;
DialMediaRouteProvider::MediaSinkQuery::~MediaSinkQuery() = default;

}  // namespace media_router
