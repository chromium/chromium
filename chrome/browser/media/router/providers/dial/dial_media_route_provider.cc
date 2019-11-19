// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/dial/dial_media_route_provider_metrics.h"
#include "chrome/common/media_router/media_source.h"
#include "url/origin.h"

namespace media_router {

namespace {

url::Origin CreateOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

void ReportParseError(DialParseMessageResult result,
                      const std::string& error_message) {
  DCHECK_NE(result, DialParseMessageResult::kSuccess);
  DVLOG(2) << "Failed to parse DIAL internal message: " << error_message;
  DialMediaRouteProviderMetrics::RecordParseMessageResult(result);
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

  // |activity_manager_| might have already been set in tests.
  if (!activity_manager_)
    activity_manager_ = std::make_unique<DialActivityManager>();

  message_sender_ =
      std::make_unique<BufferedMessageSender>(media_router_.get());

  // TODO(crbug.com/816702): This needs to be set properly according to sinks
  // discovered.
  media_router_->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::DIAL,
      mojom::MediaRouter::SinkAvailability::PER_SOURCE);
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
                                         int32_t tab_id,
                                         base::TimeDelta timeout,
                                         bool incognito,
                                         CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Create Route " << media_source << " for [" << sink_id << "]";

  const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    std::move(callback).Run(base::nullopt, nullptr, "Unknown sink " + sink_id,
                            RouteRequestResult::SINK_NOT_FOUND);
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kSinkNotFound);
    return;
  }

  auto activity =
      DialActivity::From(presentation_id, *sink, media_source, incognito);
  if (!activity) {
    std::move(callback).Run(base::nullopt, nullptr,
                            "Unsupported source " + media_source,
                            RouteRequestResult::NO_SUPPORTED_PROVIDER);
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kUnsupportedSource);
    return;
  }

  const MediaRoute::Id& route_id = activity->route.media_route_id();
  if (activity_manager_->GetActivity(route_id) ||
      activity_manager_->GetActivityBySinkId(sink_id)) {
    std::move(callback).Run(base::nullopt, nullptr, "Activity already exists",
                            RouteRequestResult::ROUTE_ALREADY_EXISTS);
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kRouteAlreadyExists);
    return;
  }

  activity_manager_->AddActivity(*activity);
  std::move(callback).Run(activity->route, nullptr, base::nullopt,
                          RouteRequestResult::OK);

  // When a custom DIAL launch request is received, DialMediaRouteProvider will
  // create a MediaRoute immediately in order to start exchanging messages with
  // the Cast SDK to complete the launch sequence. The first messages that the
  // MRP needs to send are the RECEIVER_ACTION and NEW_SESSION.
  std::vector<mojom::RouteMessagePtr> messages;
  messages.emplace_back(internal_message_util_.CreateReceiverActionCastMessage(
      activity->launch_info, *sink));
  messages.emplace_back(internal_message_util_.CreateNewSessionMessage(
      activity->launch_info, *sink));
  DVLOG(2) << "Sending RECEIVER_ACTION and NEW_SESSION for route " << route_id;
  message_sender_->SendMessages(route_id, std::move(messages));
}

void DialMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t tab_id,
                                       base::TimeDelta timeout,
                                       bool incognito,
                                       JoinRouteCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      base::nullopt, nullptr, std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
}

void DialMediaRouteProvider::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      base::nullopt, nullptr, std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
}

void DialMediaRouteProvider::TerminateRoute(const std::string& route_id,
                                            TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "TerminateRoute " << route_id;

  const DialActivity* activity = activity_manager_->GetActivity(route_id);
  if (!activity) {
    DVLOG(2) << "No activity record found with route_id " << route_id;
    std::move(callback).Run("Activity not found",
                            RouteRequestResult::ROUTE_NOT_FOUND);
    DialMediaRouteProviderMetrics::RecordTerminateRouteResult(
        DialTerminateRouteResult::kRouteNotFound);
    return;
  }

  const MediaRoute& route = activity->route;
  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(route.media_sink_id());
  if (!sink) {
    DVLOG(2) << __func__ << ": Sink not found: " << route.media_sink_id();
    std::move(callback).Run("Sink not found",
                            RouteRequestResult::SINK_NOT_FOUND);
    DialMediaRouteProviderMetrics::RecordTerminateRouteResult(
        DialTerminateRouteResult::kSinkNotFound);
    return;
  }

  DoTerminateRoute(*activity, *sink, std::move(callback));
}

void DialMediaRouteProvider::SendRouteMessage(const std::string& media_route_id,
                                              const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetDataDecoder().ParseJson(
      message,
      base::BindRepeating(&DialMediaRouteProvider::HandleParsedRouteMessage,
                          weak_ptr_factory_.GetWeakPtr(), media_route_id));
}

void DialMediaRouteProvider::HandleParsedRouteMessage(
    const MediaRoute::Id& route_id,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    ReportParseError(DialParseMessageResult::kParseError, *result.error);
    return;
  }

  std::string error;
  std::unique_ptr<DialInternalMessage> internal_message =
      DialInternalMessage::From(std::move(*result.value), &error);
  if (!internal_message) {
    ReportParseError(DialParseMessageResult::kInvalidMessage, error);
    return;
  }

  DialMediaRouteProviderMetrics::RecordParseMessageResult(
      DialParseMessageResult::kSuccess);

  const DialActivity* activity = activity_manager_->GetActivity(route_id);
  if (!activity) {
    DVLOG(2) << "No activity record found with route_id " << route_id;
    return;
  }

  const MediaRoute& route = activity->route;
  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(route.media_sink_id());
  if (!sink) {
    DVLOG(2) << __func__ << ": Sink not found: " << route.media_sink_id();
    return;
  }

  DVLOG(2) << __func__ << ": Recieved message from:" << route_id;
  // TODO(https://crbug.com/816628): Investigate whether the direct use of
  // PresentationConnection in this class to communicate with the SDK client can
  // result in eliminating the need for CLIENT_CONNECT messages.
  if (internal_message->type == DialInternalMessageType::kClientConnect) {
    HandleClientConnect(*activity, *sink);
  } else if (internal_message->type ==
             DialInternalMessageType::kCustomDialLaunch) {
    HandleCustomDialLaunchResponse(*activity, *internal_message);
  } else if (DialInternalMessageUtil::IsStopSessionMessage(*internal_message)) {
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
    // Note: this leaves the route in a stuck state; the client must terminate
    // the route. Maybe we should clean up the route here.
    DVLOG(2) << __func__ << ": unable to get app info for " << route_id;
    DialMediaRouteProviderMetrics::RecordCreateRouteResult(
        DialCreateRouteResult::kAppInfoNotFound);
    return;
  }

  // Check if activity still exists.
  auto* activity = activity_manager_->GetActivity(route_id);
  if (!activity) {
    DVLOG(2) << __func__ << ": activity no longer exists: " << route_id;
    return;
  }

  auto* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    // TODO(imcheng): We should remove the route when the sink is removed.
    DVLOG(2) << __func__ << ": sink no longer exists: " << sink_id;
    return;
  }

  auto message_and_seq_number =
      internal_message_util_.CreateCustomDialLaunchMessage(
          activity->launch_info, *sink, *result.app_info);
  pending_dial_launches_.insert(message_and_seq_number.second);
  if (pending_dial_launches_.size() > kMaxPendingDialLaunches) {
    DVLOG(2) << "Max pending DIAL launches reached; dropping "
             << *pending_dial_launches_.begin();
    pending_dial_launches_.erase(pending_dial_launches_.begin());
  }

  DVLOG(2) << "Sending CUSTOM_DIAL_LAUNCH message for route " << route_id;

  std::vector<mojom::RouteMessagePtr> messages;
  messages.emplace_back(std::move(message_and_seq_number.first));
  message_sender_->SendMessages(route_id, std::move(messages));
}

void DialMediaRouteProvider::HandleCustomDialLaunchResponse(
    const DialActivity& activity,
    const DialInternalMessage& message) {
  if (!pending_dial_launches_.erase(message.sequence_number)) {
    DVLOG(2) << __func__
             << ": Unknown sequence number: " << message.sequence_number;
    return;
  }

  const MediaRoute::Id& media_route_id = activity.route.media_route_id();
  activity_manager_->LaunchApp(
      media_route_id, CustomDialLaunchMessageBody::From(message),
      base::BindOnce(&DialMediaRouteProvider::HandleAppLaunchResult,
                     base::Unretained(this), media_route_id));
}

void DialMediaRouteProvider::HandleAppLaunchResult(
    const MediaRoute::Id& route_id,
    bool success) {
  DVLOG(2) << "Launch result for: " << route_id << ": " << success;
  DialMediaRouteProviderMetrics::RecordCreateRouteResult(
      success ? DialCreateRouteResult::kSuccess
              : DialCreateRouteResult::kAppLaunchFailed);
  NotifyAllOnRoutesUpdated();
}

void DialMediaRouteProvider::DoTerminateRoute(const DialActivity& activity,
                                              const MediaSinkInternal& sink,
                                              TerminateRouteCallback callback) {
  const MediaRoute::Id& route_id = activity.route.media_route_id();
  DVLOG(2) << "Terminating route " << route_id;
  std::pair<base::Optional<std::string>, RouteRequestResult::ResultCode>
      can_stop_app = activity_manager_->CanStopApp(route_id);
  if (can_stop_app.second == RouteRequestResult::OK) {
    std::vector<mojom::RouteMessagePtr> messages;
    messages.emplace_back(
        internal_message_util_.CreateReceiverActionStopMessage(
            activity.launch_info, sink));
    message_sender_->SendMessages(route_id, std::move(messages));
    activity_manager_->StopApp(
        route_id,
        base::BindOnce(&DialMediaRouteProvider::HandleStopAppResult,
                       base::Unretained(this), route_id, std::move(callback)));
  } else {
    std::move(callback).Run(can_stop_app.first, can_stop_app.second);
  }
}

void DialMediaRouteProvider::HandleStopAppResult(
    const MediaRoute::Id& route_id,
    TerminateRouteCallback callback,
    const base::Optional<std::string>& message,
    RouteRequestResult::ResultCode result_code) {
  DVLOG(2) << __func__ << ": " << route_id
           << ", result: " << static_cast<int>(result_code);
  if (result_code == RouteRequestResult::OK) {
    media_router_->OnPresentationConnectionStateChanged(
        route_id, mojom::MediaRouter::PresentationConnectionState::TERMINATED);
    NotifyAllOnRoutesUpdated();
    DialMediaRouteProviderMetrics::RecordTerminateRouteResult(
        DialTerminateRouteResult::kSuccess);
  } else {
    DialMediaRouteProviderMetrics::RecordTerminateRouteResult(
        DialTerminateRouteResult::kStopAppFailed);
  }
  std::move(callback).Run(message, result_code);
}

void DialMediaRouteProvider::NotifyAllOnRoutesUpdated() {
  auto routes = activity_manager_->GetRoutes();
  for (const auto& query : media_route_queries_)
    NotifyOnRoutesUpdated(query, routes);
}

void DialMediaRouteProvider::NotifyOnRoutesUpdated(
    const MediaSource::Id& source_id,
    const std::vector<MediaRoute>& routes) {
  DVLOG(2) << __func__ << ": source_id: " << source_id
           << ", # routes: " << routes.size();
  media_router_->OnRoutesUpdated(MediaRouteProviderId::DIAL, routes, source_id,
                                 /* joinable_route_ids */ {});
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

void DialMediaRouteProvider::StartObservingMediaRoutes(
    const std::string& media_source) {
  media_route_queries_.insert(media_source);

  // Return current set of routes.
  auto routes = activity_manager_->GetRoutes();
  if (!routes.empty())
    NotifyOnRoutesUpdated(media_source, routes);
}

void DialMediaRouteProvider::StopObservingMediaRoutes(
    const std::string& media_source) {
  media_route_queries_.erase(media_source);
}

void DialMediaRouteProvider::StartListeningForRouteMessages(
    const std::string& route_id) {
  message_sender_->StartListening(route_id);
}

void DialMediaRouteProvider::StopListeningForRouteMessages(
    const std::string& route_id) {
  message_sender_->StopListening(route_id);
}

void DialMediaRouteProvider::DetachRoute(const std::string& route_id) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::EnableMdnsDiscovery() {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::UpdateMediaSinks(const std::string& media_source) {
  media_sink_service_->OnUserGesture();
}

void DialMediaRouteProvider::SearchSinks(
    const std::string& sink_id,
    const std::string& media_source,
    mojom::SinkSearchCriteriaPtr search_criteria,
    SearchSinksCallback callback) {
  std::move(callback).Run(std::string());
}

void DialMediaRouteProvider::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::CreateMediaRouteController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    CreateMediaRouteControllerCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
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
    DVLOG(2) << "Not monitoring app " << app_name;
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
  media_router_->OnSinksReceived(MediaRouteProviderId::DIAL, source_id, sinks,
                                 origins);
}

std::vector<url::Origin> DialMediaRouteProvider::GetOrigins(
    const std::string& app_name) {
  static const base::NoDestructor<
      base::flat_map<std::string, std::vector<url::Origin>>>
      origin_white_list(
          {{"YouTube",
            {CreateOrigin("https://tv.youtube.com"),
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

  auto origins_it = origin_white_list->find(app_name);
  if (origins_it == origin_white_list->end())
    return std::vector<url::Origin>();

  return origins_it->second;
}

DialMediaRouteProvider::MediaSinkQuery::MediaSinkQuery() = default;
DialMediaRouteProvider::MediaSinkQuery::~MediaSinkQuery() = default;

}  // namespace media_router
