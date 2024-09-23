// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/test/test_media_route_provider.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

namespace {

const char kPresentationURL2UATestOrigin[] = "https://www.example.com";

bool IsValidSource(const std::string& source_urn) {
  return (source_urn.find("test:") == 0 ||
          IsValidStandardPresentationSource(source_urn));
}

bool Is1UAPresentationSource(const std::string& source_urn) {
  return (IsValidStandardPresentationSource(source_urn) &&
          source_urn.find(kPresentationURL2UATestOrigin) != 0);
}

}  // namespace

const mojom::MediaRouteProviderId TestMediaRouteProvider::kProviderId =
    mojom::MediaRouteProviderId::TEST;

TestMediaRouteProvider::TestMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router)
    : receiver_(this, std::move(receiver)),
      media_router_(std::move(media_router)) {
  SetSinks();
}

TestMediaRouteProvider::~TestMediaRouteProvider() = default;

void TestMediaRouteProvider::SetSinks() {
  MediaSinkInternal sink_internal_1;
  MediaSinkInternal sink_internal_2;
  MediaSink sink1{CreateCastSink("id1", "test-sink-1")};
  MediaSink sink2{CreateCastSink("id2", "test-sink-2")};
  sink1.set_provider_id(kProviderId);
  sink2.set_provider_id(kProviderId);
  sink_internal_1.set_sink(sink1);
  sink_internal_2.set_sink(sink2);
  sinks_ = {sink_internal_1, sink_internal_2};
}

void TestMediaRouteProvider::CreateRoute(const std::string& media_source,
                                         const std::string& sink_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         int32_t frame_tree_node_id,
                                         base::TimeDelta timeout,
                                         CreateRouteCallback callback) {
  if (!route_error_message_.empty()) {
    std::move(callback).Run(std::nullopt, nullptr, route_error_message_,
                            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
  } else if (!delay_.is_zero()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestMediaRouteProvider::CreateRouteTimeOut,
                       GetWeakPtr(), std::move(callback)),
        delay_);
  } else {
    DVLOG(2) << "CreateRoute with origin: " << origin
             << " and FrameTreeNode ID " << frame_tree_node_id;
    MediaRoute route(presentation_id, MediaSource(media_source), sink_id,
                     std::string("Test Route"), true);
    route.set_presentation_id(presentation_id);
    route.set_controller_type(RouteControllerType::kGeneric);
    if (Is1UAPresentationSource(media_source)) {
      route.set_local_presentation(true);
    }
    const std::string& route_id = route.media_route_id();
    routes_[route_id] = route;
    presentation_ids_to_routes_[presentation_id] = route;

    media_router_->OnPresentationConnectionStateChanged(
        route_id, blink::mojom::PresentationConnectionState::CONNECTED);
    media_router_->OnRoutesUpdated(kProviderId, GetMediaRoutes());
    std::move(callback).Run(routes_[route_id], nullptr, std::nullopt,
                            mojom::RouteRequestResultCode::OK);
  }
}

void TestMediaRouteProvider::CreateRouteTimeOut(CreateRouteCallback callback) {
  std::move(callback).Run(std::nullopt, nullptr, std::nullopt,
                          mojom::RouteRequestResultCode::TIMED_OUT);
}

void TestMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t frame_tree_node_id,
                                       base::TimeDelta timeout,
                                       JoinRouteCallback callback) {
  if (!route_error_message_.empty()) {
    std::move(callback).Run(std::nullopt, nullptr, route_error_message_,
                            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
    return;
  }
  if (!IsValidSource(media_source)) {
    std::move(callback).Run(std::nullopt, nullptr,
                            std::string("The media source is invalid."),
                            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
    return;
  }
  auto pos = presentation_ids_to_routes_.find(presentation_id);
  if (pos == presentation_ids_to_routes_.end()) {
    std::move(callback).Run(std::nullopt, nullptr,
                            std::string("Presentation does not exist."),
                            mojom::RouteRequestResultCode::UNKNOWN_ERROR);
  } else {
    MediaRoute& existing_route = pos->second;
    std::move(callback).Run(existing_route, nullptr,
                            std::string("Successfully joined session"),
                            mojom::RouteRequestResultCode::OK);
  }
}

void TestMediaRouteProvider::TerminateRoute(const std::string& route_id,
                                            TerminateRouteCallback callback) {
  auto it = routes_.find(route_id);
  if (it == routes_.end()) {
    std::move(callback).Run(std::string("Route not found in test provider"),
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }
  presentation_ids_to_routes_.erase(it->second.presentation_id());
  routes_.erase(it);
  media_router_->OnPresentationConnectionStateChanged(
      route_id, blink::mojom::PresentationConnectionState::TERMINATED);
  media_router_->OnRoutesUpdated(kProviderId, GetMediaRoutes());
  std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
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
    media_router_->OnRoutesUpdated(kProviderId, GetMediaRoutes());
  } else {
    std::string response = "Pong: " + message;
    std::vector<mojom::RouteMessagePtr> messages;
    messages.emplace_back(mojom::RouteMessage::New(
        mojom::RouteMessage::Type::TEXT, response, std::nullopt));
    media_router_->OnRouteMessagesReceived(media_route_id, std::move(messages));
  }
}

void TestMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  NOTREACHED_IN_MIGRATION()
      << "Route " << media_route_id << " does not support sending binary data.";
}

void TestMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  if (base::Contains(unsupported_media_sources_, media_source))
    sinks_ = {};
  media_router_->OnSinksReceived(kProviderId, media_source, sinks_, {});
}

void TestMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {}

void TestMediaRouteProvider::StartObservingMediaRoutes() {}

void TestMediaRouteProvider::DetachRoute(const std::string& route_id) {
  media_router_->OnPresentationConnectionClosed(
      route_id, blink::mojom::PresentationConnectionCloseReason::CLOSED,
      "Close route");
}

void TestMediaRouteProvider::EnableMdnsDiscovery() {}

void TestMediaRouteProvider::DiscoverSinksNow() {}

void TestMediaRouteProvider::BindMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    BindMediaControllerCallback callback) {
  std::move(callback).Run(false);
}

void TestMediaRouteProvider::GetState(GetStateCallback callback) {
  std::move(callback).Run(nullptr);
}

std::vector<MediaRoute> TestMediaRouteProvider::GetMediaRoutes() {
  std::vector<MediaRoute> route_list;
  for (const auto& route : routes_) {
    route_list.push_back(route.second);
  }
  return route_list;
}

void TestMediaRouteProvider::CaptureOffScreenTab(
    content::WebContents* web_contents,
    GURL source_urn,
    std::string& presentation_id) {
  offscreen_tab_ =
      std::make_unique<OffscreenTab>(this, web_contents->GetBrowserContext());
  offscreen_tab_->Start(source_urn, gfx::Size(180, 180), presentation_id);
}

bool TestMediaRouteProvider::HasRoutes() const {
  return !routes_.empty();
}

void TestMediaRouteProvider::TearDown() {
  // An OffscreenTab observes its Profile*, and must be destroyed before
  // Profiles.
  if (offscreen_tab_)
    offscreen_tab_.reset();
}

void TestMediaRouteProvider::DestroyTab(OffscreenTab* tab) {
  if (offscreen_tab_ && offscreen_tab_.get() == tab)
    offscreen_tab_.reset();
}

}  // namespace media_router
