// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_app_availability_tracker.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"

using cast_channel::GetAppAvailabilityResult;

namespace media_router {

CastAppAvailabilityTracker::CastAppAvailabilityTracker() = default;
CastAppAvailabilityTracker::~CastAppAvailabilityTracker() = default;

base::flat_set<std::string> CastAppAvailabilityTracker::RegisterSource(
    const CastMediaSource& source) {
  if (base::Contains(registered_sources_, source.source_id()))
    return base::flat_set<std::string>();

  registered_sources_.emplace(source.source_id(), source);

  base::flat_set<std::string> new_app_ids;
  for (const auto& app_info : source.app_infos()) {
    const auto& app_id = app_info.app_id;
    if (++registration_count_by_app_id_[app_id] == 1)
      new_app_ids.insert(app_id);
  }
  return new_app_ids;
}

void CastAppAvailabilityTracker::UnregisterSource(
    const CastMediaSource& source) {
  auto it = registered_sources_.find(source.source_id());
  if (it == registered_sources_.end())
    return;

  for (const auto& app_info : it->second.app_infos()) {
    const std::string& app_id = app_info.app_id;
    auto count_it = registration_count_by_app_id_.find(app_id);
    CHECK(count_it != registration_count_by_app_id_.end(),
          base::NotFatalUntil::M130);
    if (--(count_it->second) == 0)
      registration_count_by_app_id_.erase(count_it);
  }

  registered_sources_.erase(it);
}

std::vector<CastMediaSource> CastAppAvailabilityTracker::UpdateAppAvailability(
    const MediaSinkInternal& sink,
    const std::string& app_id,
    CastAppAvailabilityTracker::AppAvailability availability) {
  auto& availabilities =
      (*capabilities_and_availabilities_.emplace(sink.id(), sink).first)
          .second.availabilities;
  auto it = availabilities.find(app_id);

  GetAppAvailabilityResult old_availability =
      it != availabilities.end() ? it->second.first
                                 : GetAppAvailabilityResult::kUnknown;
  GetAppAvailabilityResult new_availability = availability.first;

  // Updated if status changes from/to kAvailable.
  bool updated = (old_availability == GetAppAvailabilityResult::kAvailable ||
                  new_availability == GetAppAvailabilityResult::kAvailable) &&
                 old_availability != new_availability;
  availabilities[app_id] = availability;

  if (!updated)
    return std::vector<CastMediaSource>();

  std::vector<CastMediaSource> affected_sources;
  for (const auto& source : registered_sources_) {
    if (source.second.ContainsApp(app_id))
      affected_sources.push_back(source.second);
  }
  return affected_sources;
}

std::vector<CastMediaSource> CastAppAvailabilityTracker::RemoveResultsForSink(
    const MediaSink::Id& sink_id) {
  auto affected_sources = GetSupportedSources(sink_id);
  capabilities_and_availabilities_.erase(sink_id);
  return affected_sources;
}

std::vector<CastMediaSource> CastAppAvailabilityTracker::GetSupportedSources(
    const MediaSink::Id& sink_id) const {
  auto it = capabilities_and_availabilities_.find(sink_id);
  if (it == capabilities_and_availabilities_.end())
    return std::vector<CastMediaSource>();

  // Find all app IDs that are available on the sink.
  std::vector<std::string> supported_app_ids;
  for (const auto& availability : it->second.availabilities) {
    if (availability.second.first == GetAppAvailabilityResult::kAvailable)
      supported_app_ids.push_back(availability.first);
  }

  // Find all registered sources whose query results contains the sink ID.
  std::vector<CastMediaSource> sources;
  for (const auto& source : registered_sources_) {
    if (source.second.ContainsAnyAppFrom(supported_app_ids))
      sources.push_back(source.second);
  }
  return sources;
}

CastAppAvailabilityTracker::AppAvailability
CastAppAvailabilityTracker::GetAvailability(const MediaSink::Id& sink_id,
                                            const std::string& app_id) const {
  auto availabilities_it = capabilities_and_availabilities_.find(sink_id);
  if (availabilities_it == capabilities_and_availabilities_.end())
    return {GetAppAvailabilityResult::kUnknown, base::TimeTicks()};

  const auto& availability_map = availabilities_it->second.availabilities;
  auto availability_it = availability_map.find(app_id);
  if (availability_it == availability_map.end())
    return {GetAppAvailabilityResult::kUnknown, base::TimeTicks()};

  return availability_it->second;
}

std::vector<std::string> CastAppAvailabilityTracker::GetRegisteredApps() const {
  std::vector<std::string> registered_apps;
  for (const auto& app_ids_and_count : registration_count_by_app_id_)
    registered_apps.push_back(app_ids_and_count.first);

  return registered_apps;
}

base::flat_set<MediaSink::Id> CastAppAvailabilityTracker::GetAvailableSinks(
    const CastMediaSource& source) const {
  base::flat_set<MediaSink::Id> sink_ids;
  // For each sink, check if there is at least one available app in |source|.
  for (const auto& availabilities : capabilities_and_availabilities_) {
    for (const auto& app_info : source.app_infos()) {
      const auto& availabilities_map = availabilities.second.availabilities;
      auto availability_it = availabilities_map.find(app_info.app_id);
      if (availability_it != availabilities_map.end() &&
          availability_it->second.first ==
              GetAppAvailabilityResult::kAvailable &&
          availabilities.second.capabilities.HasAll(
              app_info.required_capabilities)) {
        sink_ids.insert(availabilities.first);
        break;
      }
    }
  }
  return sink_ids;
}

CastAppAvailabilityTracker::CapabilitiesAndAvailabilityMap::
    CapabilitiesAndAvailabilityMap(const MediaSinkInternal& sink)
    : capabilities(sink.cast_data().capabilities) {}

CastAppAvailabilityTracker::CapabilitiesAndAvailabilityMap::
    CapabilitiesAndAvailabilityMap(const CapabilitiesAndAvailabilityMap&) =
        default;

CastAppAvailabilityTracker::CapabilitiesAndAvailabilityMap::
    ~CapabilitiesAndAvailabilityMap() = default;

}  // namespace media_router
