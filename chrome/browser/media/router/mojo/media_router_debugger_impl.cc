// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_debugger_impl.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/cast/constants.h"

namespace media_router {

namespace {
// TODO(b/272368609): Create MediaSource::IsMirroringSource() that checks if
// it's a desktop source, tab source, or is site init mirroring.
bool IsRouteMirroringSource(const MediaRoute& route) {
  const MediaSource source = route.media_source();
  return source.IsDesktopMirroringSource() || source.IsTabMirroringSource();
}
}  // namespace

MediaRouterDebuggerImpl::MediaRouterDebuggerImpl(MediaRouterMojoImpl& router)
    : MediaRoutesObserver(&router), router_(router) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
MediaRouterDebuggerImpl::~MediaRouterDebuggerImpl() = default;

void MediaRouterDebuggerImpl::NotifyGetMirroringStats(
    const base::Value::Dict& json_logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (MirroringStatsObserver& observer : observers_) {
    observer.OnMirroringStatsUpdated(json_logs);
  }
}

void MediaRouterDebuggerImpl::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsRtcpReportsEnabled()) {
    return;
  }
  std::vector<MediaRoute::Id> new_routes;

  for (auto& route : routes) {
    if (IsRouteMirroringSource(route)) {
      new_routes.push_back(route.media_route_id());
    }
  }
  std::vector<MediaRoute::Id> added_routes;
  std::vector<MediaRoute::Id> removed_routes;

  std::set_difference(new_routes.begin(), new_routes.end(),
                      previous_routes_.begin(), previous_routes_.end(),
                      std::inserter(added_routes, removed_routes.end()));
  std::set_difference(previous_routes_.begin(), previous_routes_.end(),
                      new_routes.begin(), new_routes.end(),
                      std::inserter(removed_routes, removed_routes.end()));

  previous_routes_ = new_routes;

  // The observer API does not guarantee that only one route is added or
  // removed. In the rare cases that multiple routes are added, start fetching
  // stats for the last mirroring session that was added.
  if (removed_routes.size() > 0) {
    OnMirroringRouteRemoved();
    return;
  }

  if (added_routes.size() > 0) {
    OnMirroringRouteAdded(added_routes.back());
    return;
  }
}

void MediaRouterDebuggerImpl::OnMirroringRouteAdded(
    const MediaRoute::Id& route_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_mirroring_route_id_ = route_id;

  // Wait a little bit before fetching stats to ensure that the route has
  // actually been created.
  ScheduleFetchMirroringStats(base::TimeDelta(base::Seconds(5)));
}

void MediaRouterDebuggerImpl::OnMirroringRouteRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_mirroring_route_id_ = absl::nullopt;
}

void MediaRouterDebuggerImpl::ScheduleFetchMirroringStats(
    const base::TimeDelta& init_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // When a mirroring route starts, create a mirroring stats fetch loop every
  // KRtcpReportInterval, which is the same interval that the logger will send
  // stats data.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MediaRouterDebuggerImpl::FetchMirroringStats,
                     weak_ptr_factory_.GetWeakPtr()),
      media::cast::kRtcpReportInterval + init_delay);
}

void MediaRouterDebuggerImpl::FetchMirroringStats() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only fetch mirroring stats if our feature is still enabled AND if the
  // current mirroring route still exits.
  if (!IsRtcpReportsEnabled() || !current_mirroring_route_id_.has_value()) {
    return;
  }

  router_.GetMirroringStats(
      current_mirroring_route_id_.value(),
      base::BindOnce(&MediaRouterDebuggerImpl::OnStatsFetched,
                     weak_ptr_factory_.GetWeakPtr()));

  ScheduleFetchMirroringStats();
}

void MediaRouterDebuggerImpl::OnStatsFetched(const base::Value json_stats_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  json_stats_cb.is_dict() ? NotifyGetMirroringStats(json_stats_cb.GetDict())
                          : NotifyGetMirroringStats(base::Value::Dict());
  ;
}

}  // namespace media_router
