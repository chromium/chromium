// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/test/test_media_route_provider.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

namespace {

bool IsValidSource(const std::string& source_urn) {
  return (source_urn.find("test:") == 0 ||
          IsValidStandardPresentationSource(source_urn));
}

void Wait(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), timeout);
  run_loop.Run();
}

}  // namespace

const MediaRouteProviderId TestMediaRouteProvider::kProviderId =
    MediaRouteProviderId::CAST;

TestMediaRouteProvider::TestMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router)
    : receiver_(this, std::move(receiver)),
      media_router_(std::move(media_router)) {
  SetSinks();
  media_router_->OnSinkAvailabilityUpdated(
      kProviderId, mojom::MediaRouter::SinkAvailability::PER_SOURCE);
}

void TestMediaRouteProvider::SetSinks() {
  MediaSinkInternal sink_internal_1;
  MediaSinkInternal sink_internal_2;
  sink_internal_1.set_sink(MediaSink("id1", "test-sink-1", SinkIconType::CAST));
  sink_internal_2.set_sink(MediaSink("id2", "test-sink-2", SinkIconType::CAST));
  sinks_ = {sink_internal_1, sink_internal_2};
}

void TestMediaRouteProvider::CreateRoute(const std::string& media_source,
                                         const std::string& sink_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         int32_t tab_id,
                                         base::TimeDelta timeout,
                                         bool incognito,
                                         CreateRouteCallback callback) {
  if (!route_error_message_.empty()) {
    std::move(callback).Run(base::nullopt, nullptr, route_error_message_,
                            RouteRequestResult::ResultCode::UNKNOWN_ERROR);
  } else {
    if (delay_ms_ > 0)
      Wait(base::TimeDelta::FromMilliseconds(delay_ms_));
    DVLOG(2) << "CreateRoute with origin: " << origin << " and tab ID "
             << tab_id;
    MediaRoute route(presentation_id, MediaSource(media_source), sink_id,
                     std::string("Test Route"), true, true);
    route.set_controller_type(RouteControllerType::kGeneric);
    route.set_off_the_record(incognito);
    const std::string& route_id = route.media_route_id();
    routes_[route_id] = route;
    presentation_ids_to_routes_[presentation_id] = route;

    media_router_->OnPresentationConnectionStateChanged(
        route_id, blink::mojom::PresentationConnectionState::CONNECTED);
    media_router_->OnRoutesUpdated(kProviderId, GetMediaRoutes(), media_source,
                                   {});

    std::move(callback).Run(route, nullptr, base::nullopt,
                            RouteRequestResult::UNKNOWN_ERROR);
  }
}

void TestMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t tab_id,
                                       base::TimeDelta timeout,
                                       bool incognito,
                                       JoinRouteCallback callback) {
  if (!route_error_message_.empty()) {
    std::move(callback).Run(base::nullopt, nullptr, route_error_message_,
                            RouteRequestResult::UNKNOWN_ERROR);
    return;
  }
  if (!IsValidSource(media_source)) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("The media source is invalid."),
                            RouteRequestResult::UNKNOWN_ERROR);
    return;
  }
  auto pos = presentation_ids_to_routes_.find(presentation_id);
  if (pos == presentation_ids_to_routes_.end()) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Presentation does not exist."),
                            RouteRequestResult::UNKNOWN_ERROR);
  } else if (pos->second.is_off_the_record() != incognito) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Off-the-record mismatch."),
                            RouteRequestResult::UNKNOWN_ERROR);
  } else {
    MediaRoute& existing_route = pos->second;
    std::move(callback).Run(existing_route, nullptr,
                            std::string("Successfully joined session"),
                            RouteRequestResult::ResultCode::OK);
  }
}

void TestMediaRouteProvider::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  if (!IsValidSource(media_source)) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("The media source is invalid."),
                            RouteRequestResult::UNKNOWN_ERROR);
    return;
  }
  auto pos = routes_.find(route_id);
  if (pos == presentation_ids_to_routes_.end()) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Presentation does not exist."),
                            RouteRequestResult::UNKNOWN_ERROR);
  } else {
    MediaRoute& existing_route = pos->second;
    std::move(callback).Run(existing_route, nullptr,
                            std::string("Connect route by route ID"),
                            RouteRequestResult::ResultCode::OK);
  }
}

void TestMediaRouteProvider::TerminateRoute(const std::string& route_id,
                                            TerminateRouteCallback callback) {
  auto it = routes_.find(route_id);
  if (it == routes_.end()) {
    std::move(callback).Run(std::string("Route not found in test provider"),
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }
  presentation_ids_to_routes_.erase(it->second.presentation_id());
  routes_.erase(it);
  media_router_->OnPresentationConnectionStateChanged(
      route_id, blink::mojom::PresentationConnectionState::TERMINATED);
  media_router_->OnRoutesUpdated(
      kProviderId, GetMediaRoutes(),
      MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id), {});
  std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
}

void TestMediaRouteProvider::SendRouteMessage(const std::string& media_route_id,
                                              const std::string& message) {
  if (close_route_with_error_on_send_) {
    auto it = routes_.find(media_route_id);
    presentation_ids_to_routes_.erase(it->second.presentation_id());
    routes_.erase(it);
    media_router_->OnPresentationConnectionClosed(
        media_route_id,
        blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR,
        "Send error. Closing connection.");
    media_router_->OnRoutesUpdated(
        kProviderId, GetMediaRoutes(),
        MediaRoute::GetMediaSourceIdFromMediaRouteId(media_route_id), {});
  } else {
    std::string response = "Pong: " + message;
    std::vector<mojom::RouteMessagePtr> messages;
    messages.emplace_back(mojom::RouteMessage::New(
        mojom::RouteMessage::Type::TEXT, response, base::nullopt));
    media_router_->OnRouteMessagesReceived(media_route_id, std::move(messages));
  }
}

void TestMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  NOTREACHED() << "Route " << media_route_id
               << " does not support sending binary data.";
}

void TestMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  media_router_->OnSinksReceived(kProviderId, media_source, sinks_, {});
}

void TestMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::StartObservingMediaRoutes(
    const std::string& media_source) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::StopObservingMediaRoutes(
    const std::string& media_source) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::StartListeningForRouteMessages(
    const std::string& route_id) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::StopListeningForRouteMessages(
    const std::string& route_id) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::DetachRoute(const std::string& route_id) {
  media_router_->OnPresentationConnectionClosed(
      route_id, blink::mojom::PresentationConnectionCloseReason::CLOSED,
      "Close route");
}

void TestMediaRouteProvider::EnableMdnsDiscovery() {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::UpdateMediaSinks(const std::string& media_source) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::CreateMediaRouteController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    CreateMediaRouteControllerCallback callback) {
  NOTIMPLEMENTED();
}

void TestMediaRouteProvider::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  NOTIMPLEMENTED();
}

std::vector<MediaRoute> TestMediaRouteProvider::GetMediaRoutes() {
  std::vector<MediaRoute> route_list;
  for (const auto& route : routes_)
    route_list.push_back(route.second);
  return route_list;
}

}  // namespace media_router
