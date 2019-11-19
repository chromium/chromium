// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"

#include "base/bind.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"

namespace media_router {

namespace {

// The minimum time that must elapse before an app availability result can be
// force refreshed.
static constexpr base::TimeDelta kRefreshThreshold =
    base::TimeDelta::FromMinutes(1);

}  // namespace

CastAppDiscoveryServiceImpl::CastAppDiscoveryServiceImpl(
    cast_channel::CastMessageHandler* message_handler,
    cast_channel::CastSocketService* socket_service,
    MediaSinkServiceBase* media_sink_service,
    const base::TickClock* clock)
    : message_handler_(message_handler),
      socket_service_(socket_service),
      media_sink_service_(media_sink_service),
      clock_(clock) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(message_handler_);
  DCHECK(socket_service_);
  DCHECK(clock_);
  socket_service_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastAppDiscoveryServiceImpl::Init,
                                base::Unretained(this)));
}

CastAppDiscoveryServiceImpl::~CastAppDiscoveryServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_sink_service_->RemoveObserver(this);
}

CastAppDiscoveryService::Subscription
CastAppDiscoveryServiceImpl::StartObservingMediaSinks(
    const CastMediaSource& source,
    const SinkQueryCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaSource::Id& source_id = source.source_id();

  // Returned cached results immediately, if available.
  base::flat_set<MediaSink::Id> cached_sink_ids =
      availability_tracker_.GetAvailableSinks(source);
  if (!cached_sink_ids.empty())
    callback.Run(source_id, GetSinksByIds(cached_sink_ids));

  auto& callback_list = sink_queries_[source_id];
  if (!callback_list) {
    callback_list = std::make_unique<SinkQueryCallbackList>();
    callback_list->set_removal_callback(base::BindRepeating(
        &CastAppDiscoveryServiceImpl::MaybeRemoveSinkQueryEntry,
        base::Unretained(this), source));

    // Note: even though we retain availability results for an app unregistered
    // from the tracker, we will refresh the results when the app is
    // re-registered.
    base::flat_set<std::string> new_app_ids =
        availability_tracker_.RegisterSource(source);
    const auto& sinks = media_sink_service_->GetSinks();
    for (const auto& app_id : new_app_ids) {
      // Note: The following logic assumes |sinks| will not change as it is
      // being iterated.
      for (const auto& sink : sinks) {
        int channel_id = sink.second.cast_data().cast_channel_id;
        cast_channel::CastSocket* socket =
            socket_service_->GetSocket(channel_id);
        if (!socket) {
          DVLOG(1) << "Socket not found for id " << channel_id;
          continue;
        }

        RequestAppAvailability(socket, app_id, sink.first);
      }
    }
  }
  return callback_list->Add(callback);
}

void CastAppDiscoveryServiceImpl::Refresh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto app_ids = availability_tracker_.GetRegisteredApps();
  const auto& sinks = media_sink_service_->GetSinks();
  // Note: The following logic assumes |sinks| will not change as it is
  // being iterated.
  for (const auto& sink : sinks) {
    for (const auto& app_id : app_ids) {
        int channel_id = sink.second.cast_data().cast_channel_id;
        cast_channel::CastSocket* socket =
            socket_service_->GetSocket(channel_id);
        if (!socket) {
          DVLOG(1) << "Socket not found for id " << channel_id;
          continue;
        }
        RequestAppAvailability(socket, app_id, sink.first);
      }
  }
}

void CastAppDiscoveryServiceImpl::MaybeRemoveSinkQueryEntry(
    const CastMediaSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = sink_queries_.find(source.source_id());
  CHECK(it != sink_queries_.end());

  if (it->second->empty()) {
    availability_tracker_.UnregisterSource(source);
    sink_queries_.erase(it);
  }
}

void CastAppDiscoveryServiceImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_sink_service_->AddObserver(this);
}

void CastAppDiscoveryServiceImpl::OnSinkAddedOrUpdated(
    const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cast_channel::CastSocket* socket =
      socket_service_->GetSocket(sink.cast_data().cast_channel_id);
  if (!socket)
    return;

  const MediaSink::Id& sink_id = sink.sink().id();

  // Any queries that currently contains this sink should be updated.
  UpdateSinkQueries(availability_tracker_.GetSupportedSources(sink_id));

  for (const std::string& app_id : availability_tracker_.GetRegisteredApps())
    RequestAppAvailability(socket, app_id, sink_id);
}

void CastAppDiscoveryServiceImpl::OnSinkRemoved(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaSink::Id& sink_id = sink.sink().id();
  UpdateSinkQueries(availability_tracker_.RemoveResultsForSink(sink_id));
}

void CastAppDiscoveryServiceImpl::RequestAppAvailability(
    cast_channel::CastSocket* socket,
    const std::string& app_id,
    const MediaSink::Id& sink_id) {
  base::TimeTicks now = clock_->NowTicks();
  if (ShouldRefreshAppAvailability(sink_id, app_id, now)) {
    message_handler_->RequestAppAvailability(
        socket, app_id,
        base::BindOnce(&CastAppDiscoveryServiceImpl::UpdateAppAvailability,
                       weak_ptr_factory_.GetWeakPtr(), now, sink_id));
  }
}

void CastAppDiscoveryServiceImpl::UpdateAppAvailability(
    base::TimeTicks start_time,
    const MediaSink::Id& sink_id,
    const std::string& app_id,
    cast_channel::GetAppAvailabilityResult availability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordAppAvailabilityResult(availability, clock_->NowTicks() - start_time);
  if (!media_sink_service_->GetSinkById(sink_id))
    return;

  DVLOG(1) << "App " << app_id << " on sink " << sink_id << " is "
           << ToString(availability);

  UpdateSinkQueries(availability_tracker_.UpdateAppAvailability(
      sink_id, app_id, {availability, clock_->NowTicks()}));
}

void CastAppDiscoveryServiceImpl::UpdateSinkQueries(
    const std::vector<CastMediaSource>& sources) {
  for (const auto& source : sources) {
    const MediaSource::Id& source_id = source.source_id();
    auto it = sink_queries_.find(source_id);
    if (it == sink_queries_.end())
      continue;
    base::flat_set<MediaSink::Id> sink_ids =
        availability_tracker_.GetAvailableSinks(source);
    it->second->Notify(source_id, GetSinksByIds(sink_ids));
  }
}

std::vector<MediaSinkInternal> CastAppDiscoveryServiceImpl::GetSinksByIds(
    const base::flat_set<MediaSink::Id>& sink_ids) const {
  std::vector<MediaSinkInternal> sinks;
  for (const auto& sink_id : sink_ids) {
    const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
    if (sink)
      sinks.push_back(*sink);
  }
  return sinks;
}

bool CastAppDiscoveryServiceImpl::ShouldRefreshAppAvailability(
    const MediaSink::Id& sink_id,
    const std::string& app_id,
    base::TimeTicks now) const {
  auto availability = availability_tracker_.GetAvailability(sink_id, app_id);
  switch (availability.first) {
    case cast_channel::GetAppAvailabilityResult::kAvailable:
      return false;
    case cast_channel::GetAppAvailabilityResult::kUnavailable:
      return now - availability.second > kRefreshThreshold;
    case cast_channel::GetAppAvailabilityResult::kUnknown:
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace media_router
