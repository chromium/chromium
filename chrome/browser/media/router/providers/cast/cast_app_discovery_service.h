// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_DISCOVERY_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_DISCOVERY_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/providers/cast/cast_app_availability_tracker.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_util.h"

namespace base {
class TickClock;
}

namespace cast_channel {
class CastMessageHandler;
class CastSocket;
class CastSocketService;
}  // namespace cast_channel

namespace media_router {

// Interface for app discovery for Cast MediaSinks.
class CastAppDiscoveryService {
 public:
  using SinkQueryFunc = void(const MediaSource::Id& source_id,
                             const std::vector<MediaSinkInternal>& sinks);
  using SinkQueryCallback = base::RepeatingCallback<SinkQueryFunc>;
  using SinkQueryCallbackList = base::CallbackList<SinkQueryFunc>;
  using Subscription = std::unique_ptr<SinkQueryCallbackList::Subscription>;

  virtual ~CastAppDiscoveryService() = default;

  // Adds a sink query for |source|. Results will be continuously returned via
  // |callback| until the returned Subscription is destroyed by the caller.
  // If there are cached results available, |callback| will be invoked before
  // this method returns.
  virtual Subscription StartObservingMediaSinks(
      const CastMediaSource& source,
      const SinkQueryCallback& callback) = 0;

  // Refreshes the state of app discovery in the service. It is suitable to call
  // this method when the user initiates a user gesture (such as opening the
  // Media Router dialog).
  virtual void Refresh() = 0;
};

// Keeps track of sink queries and listens to CastMediaSinkServiceImpl for sink
// updates, and issues app availability requests based on these signals. This
// class may be created on any sequence. All other methods must be called on the
// CastSocketService sequence.
class CastAppDiscoveryServiceImpl : public CastAppDiscoveryService,
                                    public MediaSinkServiceBase::Observer {
 public:
  CastAppDiscoveryServiceImpl(cast_channel::CastMessageHandler* message_handler,
                              cast_channel::CastSocketService* socket_service,
                              MediaSinkServiceBase* media_sink_service,
                              const base::TickClock* clock);
  ~CastAppDiscoveryServiceImpl() override;

  // CastAppDiscoveryService implementation.
  Subscription StartObservingMediaSinks(
      const CastMediaSource& source,
      const SinkQueryCallback& callback) override;

  // Reissues app availability requests for currently registered (sink, app_id)
  // pairs whose status is kUnavailable or kUnknown.
  void Refresh() override;

 private:
  friend class CastAppDiscoveryServiceImplTest;

  // Called on construction. Registers an observer with |media_sink_service_|.
  void Init();

  // MediaSinkServiceBase::Observer
  void OnSinkAddedOrUpdated(const MediaSinkInternal& sink) override;
  void OnSinkRemoved(const MediaSinkInternal& sink) override;

  // Issues an app availability request for |app_id| to the sink given by
  // |sink_id| via |socket|.
  void RequestAppAvailability(cast_channel::CastSocket* socket,
                              const std::string& app_id,
                              const MediaSink::Id& sink_id);

  // Updates the availability result for |sink_id| and |app_id| with |result|,
  // and notifies callbacks with updated sink query results.
  // |start_time| is the time when the app availability request was made, and
  // is used for metrics.
  void UpdateAppAvailability(base::TimeTicks start_time,
                             const MediaSink::Id& sink_id,
                             const std::string& app_id,
                             cast_channel::GetAppAvailabilityResult result);

  // Updates the sink query results for |sources|.
  void UpdateSinkQueries(const std::vector<CastMediaSource>& sources);

  // Removes the entry from |sink_queries_| if there are no more callbacks
  // associated with |source|.
  void MaybeRemoveSinkQueryEntry(const CastMediaSource& source);

  // Gets a list of sinks corresponding to |sink_ids|.
  std::vector<MediaSinkInternal> GetSinksByIds(
      const base::flat_set<MediaSink::Id>& sink_ids) const;

  // Returns true if an app availability request should be issued for |sink_id|
  // and |app_id|. |now| is used for checking whether previously cached results
  // should be refreshed.
  bool ShouldRefreshAppAvailability(const MediaSink::Id& sink_id,
                                    const std::string& app_id,
                                    base::TimeTicks now) const;

  // Registered sink queries and their associated callbacks.
  base::flat_map<MediaSource::Id, std::unique_ptr<SinkQueryCallbackList>>
      sink_queries_;

  cast_channel::CastMessageHandler* const message_handler_;
  cast_channel::CastSocketService* const socket_service_;
  MediaSinkServiceBase* const media_sink_service_;

  CastAppAvailabilityTracker availability_tracker_;

  const base::TickClock* const clock_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastAppDiscoveryServiceImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CastAppDiscoveryServiceImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_DISCOVERY_SERVICE_H_
