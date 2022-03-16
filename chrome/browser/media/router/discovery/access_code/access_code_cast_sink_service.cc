// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace media_router {

namespace {

// Connect timeout value when opening a Cast socket.
const int kConnectTimeoutInSeconds = 1;

// Amount of idle time to wait before pinging the Cast device.
const int kPingIntervalInSeconds = 1;

// Amount of idle time to wait before disconnecting.
const int kLivenessTimeoutInSeconds = 2;

using SinkSource = CastDeviceCountMetrics::SinkSource;
using ChannelOpenedCallback = base::OnceCallback<void(bool)>;
constexpr char kLoggerComponent[] = "AccessCodeCastSinkService";

const base::TimeDelta kExpirationDelay = base::Milliseconds(250);
}  // namespace

AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    AccessCodeMediaRoutesObserver(
        MediaRouter* media_router,
        AccessCodeCastSinkService* access_code_sink_service)
    : MediaRoutesObserver(media_router),
      access_code_sink_service_(access_code_sink_service) {}

AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    ~AccessCodeMediaRoutesObserver() = default;

AccessCodeCastSinkService::AccessCodeCastSinkService(
    Profile* profile,
    MediaRouter* media_router,
    CastMediaSinkServiceImpl* cast_media_sink_service_impl)
    : profile_(profile),
      media_router_(media_router),
      media_routes_observer_(
          std::make_unique<AccessCodeMediaRoutesObserver>(media_router, this)),
      cast_media_sink_service_impl_(cast_media_sink_service_impl),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(profile_);
  backoff_policy_ = {
      // Number of initial errors (in sequence) to ignore before going into
      // exponential backoff.
      0,

      // Initial delay (in ms) once backoff starts.
      1 * 1000,  // 1 second,

      // Factor by which the delay will be multiplied on each subsequent
      // failure. This must be >= 1.0.
      1.0,

      // Fuzzing percentage: 50% will spread delays randomly between 50%--100%
      // of the nominal time.
      0.5,  // 50%

      // Maximum delay (in ms) during exponential backoff.
      1 * 1000,  // 1 second

      // Time to keep an entry from being discarded even when it has no
      // significant state, -1 to never discard.
      1 * 1000,  // 1 second,

      // False means that initial_delay_ms is the first delay once we start
      // exponential backoff, i.e., there is no delay after subsequent
      // successful requests.
      false,
  };
}

AccessCodeCastSinkService::AccessCodeCastSinkService(Profile* profile)
    : AccessCodeCastSinkService(
          profile,
          MediaRouterFactory::GetApiForBrowserContext(profile),
          media_router::DualMediaSinkService::GetInstance()
              ->GetCastMediaSinkServiceImpl()) {}

AccessCodeCastSinkService::~AccessCodeCastSinkService() = default;

base::WeakPtr<AccessCodeCastSinkService>
AccessCodeCastSinkService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  std::vector<MediaRoute::Id> new_routes;
  for (const auto& route : routes) {
    new_routes.push_back(route.media_route_id());
  }

  std::vector<MediaRoute::Id> removed_routes;
  std::set_difference(old_routes_.begin(), old_routes_.end(),
                      new_routes.begin(), new_routes.end(),
                      std::inserter(removed_routes, removed_routes.end()));
  old_routes_ = new_routes;

  // No routes were removed.
  if (removed_routes.empty())
    return;

  // There should only be 1 element in the |removed_routes| set.
  DCHECK(removed_routes.size() < 2);
  auto first = removed_routes.begin();
  MediaRoute::Id removed_route_id = *first;

  base::PostTaskAndReplyWithResult(
      access_code_sink_service_->cast_media_sink_service_impl_->task_runner()
          .get(),
      FROM_HERE,
      base::BindOnce(
          &CastMediaSinkServiceImpl::GetSinkById,
          base::Unretained(
              access_code_sink_service_->cast_media_sink_service_impl_),
          MediaRoute::GetSinkIdFromMediaRouteId(removed_route_id)),
      base::BindOnce(
          &AccessCodeCastSinkService::HandleMediaRouteDiscoveredByAccessCode,
          access_code_sink_service_->GetWeakPtr()));
}

void AccessCodeCastSinkService::HandleMediaRouteDiscoveredByAccessCode(
    const MediaSinkInternal* sink) {
  // The route Id did not correspond to a sink for some reason. Return to
  // avoid nullptr issues.
  if (!sink)
    return;

  // Check to see if route was created by an access code sink.
  if (!sink->is_cast_sink()) {
    return;
  }
  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "An Access Code Cast route has ended.", sink->id(), "", "");
  // There are two possible cases here. The common case is that a route for
  // the specified sink has been terminated by local or remote user
  // interaction. In this case, call OnAccessCodeRouteRemoved to check whether
  // the sink should now be removed due to expiration. The second case occurs
  // during discovery. It's possible that the discovery process discovered a
  // sink that already existed, and that that sink had an active route. In
  // that case, |OpenChannelIfNecessary| will have terminated that route. It
  // is important though, that we don't attempt to expire the sink in that
  // case, because the user has in fact just "discovered" it. So before
  // attempting to expire the sink, check to see whether the termination was
  // due to discovery. If so, then alert the dialog about the successful
  // discovery.
  auto it = pending_callbacks_.find(sink->id());
  if (it != pending_callbacks_.end()) {
    std::move(it->second).Run(true);
    pending_callbacks_.erase(sink->id());
  } else {
    if (sink->cast_data().discovered_by_access_code) {
      // Need to pause just a little bit before attempting to remove the sink.
      // Sometimes sinks terminate their routes and immediately start another
      // (tab content transitions for example), so wait just a little while
      // before checking to see if removing the route makes sense.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AccessCodeCastSinkService::OnAccessCodeRouteRemoved,
                         weak_ptr_factory_.GetWeakPtr(), *sink),
          kExpirationDelay);
    }
  }
}

void AccessCodeCastSinkService::OnAccessCodeRouteRemoved(
    const MediaSinkInternal& sink) {
  // If the sink is a cast sink discovered by Access Code, instantly expire
  // it from the media router after the casting session has ended. Need to be
  // careful though, because sometimes after a route is removed, another route
  // is immediately reestablished (this can occur if a tab transitions from
  // one type of content (mirroring) to another (preseentation). There was a
  // pause before this method was called, so check again to see if there's an
  // active route for this sink. Only expire the sink if a new route wasn't
  // established during the pause.
  auto routes = media_router_->GetCurrentRoutes();
  auto route_it = std::find_if(routes.begin(), routes.end(),
                               [&sink](const MediaRoute& route) {
                                 return route.media_sink_id() == sink.id();
                               });
  if (route_it == routes.end()) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "Attempting to disconnect and remove the cast sink from "
        "the media router.",
        sink.id(), "", "");
    // Only remove the sink if there is still no active routes for this sink.
    cast_media_sink_service_impl_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CastMediaSinkServiceImpl::DisconnectAndRemoveSink,
                       base::Unretained(cast_media_sink_service_impl_), sink));
  }
}

void AccessCodeCastSinkService::AddSinkToMediaRouter(
    const MediaSinkInternal& sink,
    ChannelOpenedCallback callback) {
  // Check to see if the media sink already exists in the media router.
  base::PostTaskAndReplyWithResult(
      cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                     base::Unretained(cast_media_sink_service_impl_),
                     sink.id()),
      base::BindOnce(&AccessCodeCastSinkService::OpenChannelIfNecessary,
                     weak_ptr_factory_.GetWeakPtr(), sink,
                     std::move(callback)));
}

void AccessCodeCastSinkService::OpenChannelIfNecessary(
    const MediaSinkInternal& sink,
    ChannelOpenedCallback callback,
    bool has_sink) {
  if (has_sink) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The sink already exists in the media router, no channel "
        "needs to be opened.",
        sink.id(), "", "");
    // Check to see if this sink has an active route. If so, we need to
    // terminate the route before alerting the dialog to discovery success.
    // This is because any attempt to start a route on a sink that already has
    // one won't be successful.
    auto routes = media_router_->GetCurrentRoutes();
    auto route_it = std::find_if(routes.begin(), routes.end(),
                                 [&sink](const MediaRoute& route) {
                                   return route.media_sink_id() == sink.id();
                                 });
    if (route_it != routes.end()) {
      media_router_->GetLogger()->LogInfo(
          mojom::LogCategory::kDiscovery, kLoggerComponent,
          "There was an existing route when discovery occurred, attempting to "
          "terminate it.",
          sink.id(), "", "");
      media_router_->TerminateRoute(route_it->media_route_id());
      pending_callbacks_.emplace(sink.id(), std::move(callback));
    } else {
      std::move(callback).Run(true);
    }
    return;
  }
  auto backoff_entry = std::make_unique<net::BackoffEntry>(&backoff_policy_);
  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "Attempting to open a cast channel.", sink.id(), "", "");
  cast_media_sink_service_impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel,
                     base::Unretained(cast_media_sink_service_impl_), sink,
                     std::move(backoff_entry), SinkSource::kAccessCode,
                     std::move(callback), CreateCastSocketOpenParams(sink)));
}

cast_channel::CastSocketOpenParams
AccessCodeCastSinkService::CreateCastSocketOpenParams(
    const MediaSinkInternal& sink) {
  return cast_channel::CastSocketOpenParams(
      sink.cast_data().ip_endpoint, base::Seconds(kConnectTimeoutInSeconds),
      base::Seconds(kLivenessTimeoutInSeconds),
      base::Seconds(kPingIntervalInSeconds),
      cast_channel::CastDeviceCapability::NONE);
}

void AccessCodeCastSinkService::Shutdown() {
  // There's no guarantee that MediaRouter is still in the
  // MediaRoutesObserver. |media_routes_observer_| accesses MediaRouter in its
  // dtor. Since MediaRouter and |this| are both KeyedServices, we must not
  // access MediaRouter in the dtor of |this|, so we do it here.
  media_routes_observer_.reset();
}

}  // namespace media_router
