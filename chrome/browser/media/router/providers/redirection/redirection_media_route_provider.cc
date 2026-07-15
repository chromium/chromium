// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/redirection/redirection_media_route_provider.h"

#include <utility>

#include "base/notreached.h"
#include "base/win/win_util.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "url/origin.h"

namespace media_router {

namespace {

constexpr char kSinkId[] = "redirection";
constexpr char kDefaultSinkName[] = "Redirection";

}  // namespace

// static
const mojom::MediaRouteProviderId RedirectionMediaRouteProvider::kProviderId =
    mojom::MediaRouteProviderId::REDIRECTION;

RedirectionMediaRouteProvider::RedirectionMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router)
    : receiver_(this, std::move(receiver)),
      media_router_(std::move(media_router)) {
  MediaSink sink(kSinkId, kDefaultSinkName, SinkIconType::GENERIC, kProviderId);
  sink_.set_sink(sink);
}

RedirectionMediaRouteProvider::~RedirectionMediaRouteProvider() = default;

void RedirectionMediaRouteProvider::CreateRoute(
    const std::string& media_source,
    const std::string& sink_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t frame_tree_node_id,
    base::TimeDelta timeout,
    CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(sink_id, kSinkId);
  if (!IsRedirectionAvailable()) {
    std::move(callback).Run(std::nullopt, nullptr,
                            std::string("Redirection not available"),
                            mojom::RouteRequestResultCode::SINK_NOT_FOUND);
    return;
  }

  // Redirection is a browser-level session exposed only through the Cast UI, so
  // there can never be an existing route when a new one is created.
  CHECK(active_route_id_.empty());

  // Launch the utility-process service so it is ready by the time the media
  // pipeline is wired up. The MMR media pipeline itself is not implemented yet.
  redirection_service_host_ = CreateServiceHost();
  redirection_service_host_->Start();

  const MediaRoute::Id route_id = MediaRoute::GetMediaRouteId(
      presentation_id, sink_id, MediaSource(media_source));
  MediaRoute route(route_id, MediaSource(media_source), sink_id,
                   std::string(kDefaultSinkName), /*is_local=*/true);
  route.set_presentation_id(presentation_id);

  active_route_id_ = route_id;
  std::move(callback).Run(route, nullptr, std::nullopt,
                          mojom::RouteRequestResultCode::OK);
}

void RedirectionMediaRouteProvider::JoinRoute(
    const std::string& media_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t frame_tree_node_id,
    base::TimeDelta timeout,
    JoinRouteCallback callback) {
  std::move(callback).Run(std::nullopt, nullptr,
                          std::string("Redirection join is not supported"),
                          mojom::RouteRequestResultCode::UNKNOWN_ERROR);
}

void RedirectionMediaRouteProvider::TerminateRoute(
    const std::string& route_id,
    TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (route_id != active_route_id_) {
    std::move(callback).Run(std::string("Route not found"),
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  TerminateActiveRoute();
  std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
}

void RedirectionMediaRouteProvider::SendRouteMessage(
    const std::string& media_route_id,
    const std::string& message) {
  // Redirection routes are never exposed to page script, so this can't be hit.
  NOTREACHED();
}

void RedirectionMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  // Redirection routes are never exposed to page script, so this can't be hit.
  NOTREACHED();
}

void RedirectionMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Redirection is a browser-level session triggered only by the user (or
  // policy). Expose the sink solely through the tab-mirroring query that backs
  // the Cast UI, so it is never reachable from page script (Remote Playback) or
  // the Global Media Controls.
  if (!MediaSource(media_source).IsTabMirroringSource() ||
      !IsRedirectionAvailable()) {
    return;
  }

  media_router_->OnSinksReceived(kProviderId, media_source, {sink_}, {});
}

void RedirectionMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {}

void RedirectionMediaRouteProvider::StartObservingMediaRoutes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_router_->OnRoutesUpdated(kProviderId, {});
}

void RedirectionMediaRouteProvider::DetachRoute(const std::string& route_id) {
  // Redirection routes have no PresentationConnection to detach.
  NOTREACHED();
}

void RedirectionMediaRouteProvider::DiscoverSinksNow() {
  // The synthetic redirection sink is static, so there is nothing to discover.
}

void RedirectionMediaRouteProvider::BindMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    BindMediaControllerCallback callback) {
  std::move(callback).Run(false);
}

void RedirectionMediaRouteProvider::GetState(GetStateCallback callback) {
  std::move(callback).Run(mojom::ProviderStatePtr());
}

bool RedirectionMediaRouteProvider::IsRedirectionAvailable() const {
  // Redirection is only supported inside an RDP session.
  return base::win::IsCurrentSessionRemote();
}

std::unique_ptr<RedirectionServiceHost>
RedirectionMediaRouteProvider::CreateServiceHost() {
  return std::make_unique<RedirectionServiceHost>();
}

void RedirectionMediaRouteProvider::TerminateActiveRoute() {
  if (active_route_id_.empty()) {
    return;
  }

  // Destroying the host closes the Mojo remote and tears down the utility
  // process.
  redirection_service_host_.reset();
  active_route_id_.clear();
  media_router_->OnRoutesUpdated(kProviderId, {});
}

}  // namespace media_router
