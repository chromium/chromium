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
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace media_router {
using SinkSource = CastDeviceCountMetrics::SinkSource;
using ChannelOpenedCallback = base::OnceCallback<void(bool)>;

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
      media_routes_observer_(
          std::make_unique<AccessCodeMediaRoutesObserver>(media_router, this)),
      cast_media_sink_service_impl_(cast_media_sink_service_impl) {
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
      10 * 1000,  // 10 seconds

      // Time to keep an entry from being discarded even when it has no
      // significant state, -1 to never discard. (Not applicable.)
      -1,

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
  // The route Id did not correspond to a sink for some reason. Return to avoid
  // nullptr issues.
  if (!sink)
    return;

  // Check to see if route was created by an access code sink.
  if (!sink->is_cast_sink()) {
    return;
  }
  if (sink->cast_data().discovered_by_access_code) {
    OnAccessCodeRouteRemoved(*sink);
  }
}

void AccessCodeCastSinkService::OnAccessCodeRouteRemoved(
    const MediaSinkInternal& sink) {
  // If the sink is a cast sink discovered by Access Code, instantly expire
  // it from the media router after the casting session has ended.
  cast_media_sink_service_impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::DisconnectAndRemoveSink,
                     base::Unretained(cast_media_sink_service_impl_), sink));
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
    std::move(callback).Run(true);
    return;
  }
  auto backoff_entry = std::make_unique<net::BackoffEntry>(&backoff_policy_);
  cast_media_sink_service_impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel,
                                base::Unretained(cast_media_sink_service_impl_),
                                sink, std::move(backoff_entry),
                                SinkSource::kAccessCode, std::move(callback)));
}

void AccessCodeCastSinkService::Shutdown() {
  // There's no guarantee that MediaRouter is still in the MediaRoutesObserver.
  // |media_routes_observer_| accesses MediaRouter in its dtor. Since
  // MediaRouter and |this| are both KeyedServices, we must not access
  // MediaRouter in the dtor of |this|, so we do it here.
  media_routes_observer_.reset();
}

}  // namespace media_router
