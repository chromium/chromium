// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_AVAILABILITY_TRACKER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_AVAILABILITY_TRACKER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"

namespace media_router {

// Tracks sink queries and their extracted Cast app IDs and their availabilities
// on discovered sinks.
// Example usage:
///
// (1) A page creates a PresentationRequest with URL "cast:foo". To register the
// source to be tracked:
//   CastAppAvailabilityTracker tracker;
//   auto source = CastMediaSource::From("cast:foo");
//   auto new_app_ids = tracker.RegisterSource(*source);
//
// (2) The set of app IDs returned by the tracker can then be used by the caller
// to send an app availability request to each of the discovered sinks.
//
// (3) Once the caller knows the availability value for a (sink, app) pair, it
// may inform the tracker to update its results:
//   auto affected_sources =
//       tracker.UpdateAppAvailability(sink_id, app_id, {availability, now});
//
// (4) The tracker returns a subset of discovered sources that were affected by
// the update. The caller can then call |GetAvailableSinks()| to get the updated
// results for each affected source.
//
// (5a): At any time, the caller may call |RemoveResultsForSink()| to remove
// cached results pertaining to the sink, when it detects that a sink is removed
// or no longer valid.
//
// (5b): At any time, the caller may call |GetAvailableSinks()| (even before the
// source is registered) to determine if there are cached results available.
class CastAppAvailabilityTracker {
 public:
  // The result of an app availability request and the time when it is obtained.
  using AppAvailability =
      std::pair<cast_channel::GetAppAvailabilityResult, base::TimeTicks>;

  CastAppAvailabilityTracker();

  CastAppAvailabilityTracker(const CastAppAvailabilityTracker&) = delete;
  CastAppAvailabilityTracker& operator=(const CastAppAvailabilityTracker&) =
      delete;

  ~CastAppAvailabilityTracker();

  // Registers |source| with the tracker. Returns a list of new app IDs that
  // were previously not known to the tracker.
  base::flat_set<std::string> RegisterSource(const CastMediaSource& source);

  // Unregisters the source given by |source| with the tracker.
  void UnregisterSource(const CastMediaSource& source);

  // Updates the availability of |app_id| on |sink_id| to |availability|.
  // Returns a list of registered CastMediaSources for which the set of
  // available sinks might have been updated by this call. The caller should
  // call |GetAvailableSinks| with the returned CastMediaSources to get the
  // updated lists.
  std::vector<CastMediaSource> UpdateAppAvailability(
      const MediaSinkInternal& sink,
      const std::string& app_id,
      AppAvailability availability);

  // Removes all results associated with |sink_id|, i.e. when the sink becomes
  // invalid.
  // Returns a list of registered CastMediaSources for which the set of
  // available sinks might have been updated by this call. The caller should
  // call |GetAvailableSinks| with the returned CastMediaSources to get the
  // updated lists.
  std::vector<CastMediaSource> RemoveResultsForSink(
      const MediaSink::Id& sink_id);

  // Returns a list of registered CastMediaSources supported by |sink_id|.
  std::vector<CastMediaSource> GetSupportedSources(
      const MediaSink::Id& sink_id) const;

  // Returns the availability for |app_id| on |sink_id| and the time at
  // which the availability was determined. If availability is kUnknown, then
  // the time may be null (e.g. if an availability request was never sent).
  AppAvailability GetAvailability(const MediaSink::Id& sink_id,
                                  const std::string& app_id) const;

  // Returns a list of registered app IDs.
  std::vector<std::string> GetRegisteredApps() const;

  // Returns a list of sink IDs compatible with |source|, using the current
  // availability info.
  base::flat_set<MediaSink::Id> GetAvailableSinks(
      const CastMediaSource& source) const;

 private:
  // App ID to availability.
  using AppAvailabilityMap = base::flat_map<std::string, AppAvailability>;

  struct CapabilitiesAndAvailabilityMap {
   public:
    explicit CapabilitiesAndAvailabilityMap(const MediaSinkInternal& sink);
    CapabilitiesAndAvailabilityMap(const CapabilitiesAndAvailabilityMap&);
    ~CapabilitiesAndAvailabilityMap();

    cast_channel::CastDeviceCapabilitySet capabilities;
    AppAvailabilityMap availabilities;
  };

  // Registered sources and corresponding CastMediaSource's.
  base::flat_map<MediaSource::Id, CastMediaSource> registered_sources_;

  // App IDs tracked and the number of registered sources containing them.
  base::flat_map<std::string, int> registration_count_by_app_id_;

  // IDs and app availabilities of known sinks.
  base::flat_map<MediaSink::Id, CapabilitiesAndAvailabilityMap>
      capabilities_and_availabilities_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_AVAILABILITY_TRACKER_H_
