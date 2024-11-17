// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"

namespace media_router {

using ChannelOpenedCallback = base::OnceCallback<void(bool)>;
using AddSinkResultCode = access_code_cast::mojom::AddSinkResultCode;

bool IsAccessCodeCastEnabled();

class AccessCodeCastSinkService : public KeyedService,
                                  public DiscoveryNetworkMonitor::Observer {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  using AddSinkResultCallback = base::OnceCallback<void(
      access_code_cast::mojom::AddSinkResultCode add_sink_result,
      std::optional<MediaSink::Id> sink_id)>;

  // Use `AccessCodeCastSinkServiceFactory::GetForProfile(..)` to get
  // an instance of this service.
  explicit AccessCodeCastSinkService(Profile* profile);

  // Dependency injection constructor used for testing.
  AccessCodeCastSinkService(
      Profile* profile,
      MediaRouter* media_router,
      CastMediaSinkServiceImpl* cast_media_sink_service_impl,
      DiscoveryNetworkMonitor* network_monitor,
      PrefService* prefs,
      std::unique_ptr<AccessCodeCastPrefUpdater> pref_updater);

  AccessCodeCastSinkService(const AccessCodeCastSinkService&) = delete;
  AccessCodeCastSinkService& operator=(const AccessCodeCastSinkService&) =
      delete;

  ~AccessCodeCastSinkService() override;

  base::WeakPtr<AccessCodeCastSinkService> GetWeakPtr();

  // With the given `access_code`, make a call to the discovery server to
  // validate the device.
  // `access_code`: the access code that is sent to the discovery server.
  // `callback`: a callback sent that returns a discovery device and the status
  // of that device.
  virtual void DiscoverSink(const std::string& access_code,
                            AddSinkResultCallback callback);

  // Attempts to add a sink to the Media Router.
  // `sink`: the sink that is added to the router.
  // `callback`: a callback that tracks the status of opening a cast channel to
  // the given media sink.
  virtual void AddSinkToMediaRouter(const MediaSinkInternal& sink,
                                    AddSinkResultCallback add_sink_callback);

  static constexpr base::TimeDelta kExpirationTimerDelay = base::Seconds(20);

  // This value is used in whenever expiration timers are created. It is a
  // buffer
  // used to ensure all cast protocol steps are finished before instant
  // expiration occurs.
  static constexpr base::TimeDelta kExpirationDelay = base::Milliseconds(450);

  // Upon network changes, we remove all saved devices, and then we attempt to
  // add all saved devices again. In the case where a device is connectable
  // both before and after the network change, we must wait for the device to
  // fully be removed from media_router, or else the attempt to add it back will
  // not be successful. This buffer gives us enough time to ensure the device is
  // removed before we try to add it back.
  static constexpr base::TimeDelta kNetworkChangeBuffer = base::Seconds(4);

  // This function manually calculates the duration till expiration and
  // overrides any existing expiration timers if the duration is zero. This
  // function exists largely for edge case scenarios with instant expiration
  // that require expiration before the default kExpirationTimerDelay.
  void CheckMediaSinkForExpiration(const MediaSink::Id& sink_id);

  // Testing methods, do not use these outside of tests.
  // TODO(b/340579614) Re-factor tests so we can hide these methods again.
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    task_runner_ = task_runner;
  }

  void SetIdentityManagerForTesting(signin::IdentityManager* identity_manager);

  AccessCodeCastPrefUpdater* GetPrefUpdaterForTesting() {
    return pref_updater_.get();
  }

  const std::map<MediaSink::Id, std::unique_ptr<base::OneShotTimer>>&
  GetCurrentSessionExpirationTimersForTesting() {
    return current_session_expiration_timers_;
  }

  bool IsAccessCodeCastLacrosSyncEnabledForTesting();

  void ResetPrefUpdaterForTesting();

  void ShutdownForTesting();

  void StoreSinkInPrefsForTesting(base::OnceClosure on_sink_stored_callback,
                                  const MediaSinkInternal* sink);

  void SetExpirationTimerForTesting(const MediaSink::Id& sink_id);

  void OpenChannelIfNecessaryForTesting(const MediaSinkInternal& sink,
                                        AddSinkResultCallback add_sink_callback,
                                        bool has_sink);

  void HandleMediaRouteAddedForTesting(const MediaRoute::Id route_id,
                                       const bool is_route_local,
                                       const MediaSource media_source,
                                       const MediaSinkInternal* sink);

  void HandleMediaRouteRemovedByAccessCodeForTesting(
      const MediaSinkInternal* sink);

  void OnAccessCodeValidatedForTesting(
      AddSinkResultCallback add_sink_callback,
      std::optional<DiscoveryDevice> discovery_device,
      AddSinkResultCode result_code);

  void OnChannelOpenedResultForTesting(AddSinkResultCallback add_sink_callback,
                                       const MediaSinkInternal& sink,
                                       bool channel_opened);

  void InitializePrefUpdaterForTesting();

  void InitAllStoredDevicesForTesting();

  void CalculateDurationTillExpirationForTesting(
      const MediaSink::Id& sink_id,
      base::OnceCallback<void(base::TimeDelta)>
          on_duration_calculated_callback);

  // Testing methods that reach into the owned observer.  This is done in order
  // To keep the AccessCodeMediaRoutesObserver interface private.  Assumes
  // media_routes_observer_ is not null.
  const MediaRoute::Id& GetObserverRemovedRouteIdForTesting() {
    return media_routes_observer_->GetRemovedRouteIdForTest();
  }

  void OnObserverRoutesUpdatedForTesting(const std::vector<MediaRoute>& routes);

 private:
  class AccessCodeMediaRoutesObserver : public MediaRoutesObserver {
   public:
    AccessCodeMediaRoutesObserver(
        MediaRouter* media_router,
        AccessCodeCastSinkService* access_code_sink_service);

    AccessCodeMediaRoutesObserver(const AccessCodeMediaRoutesObserver&) =
        delete;
    AccessCodeMediaRoutesObserver& operator=(
        const AccessCodeMediaRoutesObserver&) = delete;

    ~AccessCodeMediaRoutesObserver() override;

    // Teting methods, do not use these outside of tests.
    void OnRoutesUpdatedForTesting(const std::vector<MediaRoute>& routes);

    const MediaRoute::Id& GetRemovedRouteIdForTest() {
      return removed_route_id_;
    }

   private:
    // media_router::MediaRoutesObserver:
    void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

    // Set of route ids that is updated whenever OnRoutesUpdated is called. We
    // store this value to check whether a route was removed or not.
    std::vector<MediaRoute::Id> previous_routes_;

    MediaRoute::Id removed_route_id_;

    const raw_ptr<AccessCodeCastSinkService> access_code_sink_service_;

    base::WeakPtrFactory<AccessCodeMediaRoutesObserver> weak_ptr_factory_{this};
  };
  friend class AccessCodeCastSinkServiceFactory;

  void OnAccessCodeValidated(AddSinkResultCallback add_sink_callback,
                             std::optional<DiscoveryDevice> discovery_device,
                             AddSinkResultCode result_code);

  void OnChannelOpenedResult(AddSinkResultCallback add_sink_callback,
                             const MediaSinkInternal& sink,
                             bool channel_opened);

  bool IsSinkValidAccessCodeSink(const MediaSinkInternal* sink);

  // Handles removal from media router via expiration if a route with an access
  // code cast sink has ended.
  void HandleMediaRouteRemovedByAccessCode(const MediaSinkInternal* sink);

  // Reports to metrics whenever the added route is to an access code sink.
  void HandleMediaRouteAdded(const MediaRoute::Id route_id,
                             const bool is_route_local,
                             const MediaSource media_source,
                             const MediaSinkInternal* sink);

  void OnAccessCodeRouteRemoved(const MediaSinkInternal* sink);
  void OpenChannelIfNecessary(const MediaSinkInternal& sink,
                              AddSinkResultCallback add_sink_callback,
                              bool has_sink);
  void OpenChannelWithParams(std::unique_ptr<net::BackoffEntry> backoff_entry,
                             const MediaSinkInternal& sink,
                             base::OnceCallback<void(bool)> channel_opened_cb,
                             cast_channel::CastSocketOpenParams open_params);

  // Returns a MediaRoute if the given `sink_id` corresponds to a route
  // currently active in the media router.
  std::optional<const MediaRoute> GetActiveRoute(const MediaSink::Id& sink_id);

  // Fetches and validates stored devices from the pref service. No-op if
  // `pref_updater_` hasn't been instantiated.
  void InitAllStoredDevices();

  // Invoked with validated sinks fetched from the pref service. Adds the
  // validated sinks to the Media Router and sets the expiration timers.
  // `OnStored...` is used when the sink service initializes with stored
  // devices.
  void OnStoredDevicesValidated(
      const std::vector<MediaSinkInternal>& validated_sinks);

  // Fetches devices and passes the validated devices to
  // `on_device_validated_callback`.
  void FetchAndValidateStoredDevices(
      base::OnceCallback<void(const std::vector<MediaSinkInternal>&)>
          on_device_validated_callback);

  // Iterates through `stored_sinks` fetched from the pref service and attempts
  // to validate the base::Value into a MediaSinkInternal. Removes invalid
  // devices from the pref service and passes the validated devices to
  // `on_device_validated_callback`.
  void ValidateStoredDevices(
      base::OnceCallback<void(const std::vector<MediaSinkInternal>&)>
          on_device_validated_callback,
      base::Value::Dict stored_sinks);

  // Makes a new timer entry into the expiration map. Resets the current timer
  // if it already exists.
  void InitExpirationTimers(const std::vector<MediaSinkInternal>& cast_sinks);
  void SetExpirationTimer(const MediaSink::Id& sink_id);
  void DoSetExpirationTimer(const MediaSink::Id& sink_id,
                            base::TimeDelta time_till_expiration);
  void ResetExpirationTimers();
  void CalculateDurationTillExpiration(const MediaSink::Id& sink_id,
                                       base::OnceCallback<void(base::TimeDelta)>
                                           on_duration_calculated_callback);
  void DoCalculateDurationTillExpiration(
      const MediaSink::Id& sink_id,
      base::OnceCallback<void(base::TimeDelta)> on_duration_calculated_callback,
      std::optional<base::Time> fetched_device_added_time);
  void OnExpiration(const MediaSink::Id& sink_id);

  // This function first removes itself from all the prefs and then checks to
  // see if the sink is contained within the media router, and attempts to
  // remove it if there is a sink in the media router.
  void ExpireSink(const MediaSink::Id& sink_id);

  void StoreSinkInPrefsById(const MediaSink::Id& sink_id,
                            base::OnceClosure on_sink_stored_callback);
  void StoreSinkInPrefs(base::OnceClosure on_sink_stored_callback,
                        const MediaSinkInternal* sink);
  void StoreSinkAndSetExpirationTimer(const MediaSink::Id& sink_id);

  void AddStoredDevicesToMediaRouter(
      const std::vector<MediaSinkInternal>& cast_sinks);

  // Removes the given `sink_id` from all entries in the AccessCodeCast pref
  // service.
  void RemoveSinkIdFromAllEntries(const MediaSink::Id& sink_id);
  // Removes the media sink from the MediaSinkServiceBase AND closes the cast
  // socket of that media sink. This function is a no-op if there exists a local
  // active route for the sink.
  void RemoveAndDisconnectMediaSinkFromRouter(const MediaSinkInternal* sink);
  void RemoveAndDisconnectExistingSinksOnNetwork();

  void DoCheckMediaSinkForExpiration(const MediaSink::Id& sink_id,
                                     base::TimeDelta time_till_expiration);

  void UpdateExistingSink(const MediaSinkInternal& new_sink,
                          const MediaSinkInternal* exisitng_sink);

  // DiscoveryNetworkMonitor::Observer implementation
  void OnNetworksChanged(const std::string& network_id) override;

  void ResetExpirationTimersAndInitAllStoredDevices();

  void OnDurationPrefChange();
  void OnEnabledPrefChange();
  void OnDevicesPrefChange();

  cast_channel::CastSocketOpenParams CreateCastSocketOpenParams(
      const MediaSinkInternal& sink);

  // Instantiate `pref_updater_` and call `InitAllStoredDevices()`.
  void InitializePrefUpdater();

  void LogInfo(const std::string& log_message, const std::string& sink_id);
  void LogWarning(const std::string& log_message, const std::string& sink_id);
  void LogError(const std::string& log_message, const std::string& sink_id);

  // KeyedService.
  void Shutdown() override;

  const raw_ptr<media_router::CastMediaSinkServiceImpl>
  GetCastMediaSinkServiceImpl() {
    return cast_media_sink_service_impl_;
  }

  // Owns us via the KeyedService mechanism.
  const raw_ptr<Profile> profile_;

  // There are some edge cases where the AccessCodeCastSinkService can outlive
  // the MediaRouter. This variable must be checked for validity before use.
  raw_ptr<media_router::MediaRouter> media_router_;

  // Helper class for observing the removal of MediaRoutes.
  std::unique_ptr<AccessCodeMediaRoutesObserver> media_routes_observer_;

  // Raw pointer of leaky singleton CastMediaSinkServiceImpl, which manages
  // the addition and removal of cast sinks in the Media Router. This is
  // guaranteed to be destroyed after destruction of the
  // AccessCodeCastSinkService.
  const raw_ptr<media_router::CastMediaSinkServiceImpl>
      cast_media_sink_service_impl_;

  std::unique_ptr<AccessCodeCastDiscoveryInterface> discovery_server_interface_;

  net::BackoffEntry::Policy backoff_policy_;

  // Map of sink_ids keyed to a value of expiration timers for the current
  // session (this is updated when the profile session or network is changed).
  std::map<MediaSink::Id, std::unique_ptr<base::OneShotTimer>>
      current_session_expiration_timers_;

  // For all current local routes, records the time those routes started.
  std::map<MediaRoute::Id, base::Time> current_route_start_times_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Raw pointer to DiscoveryNetworkMonitor, which is a global leaky singleton
  // and manages network change notifications.
  const raw_ptr<DiscoveryNetworkMonitor> network_monitor_;

  raw_ptr<PrefService, DanglingUntriaged> prefs_;

  std::unique_ptr<AccessCodeCastPrefUpdater> pref_updater_;

  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_ =
      nullptr;

  // This registrar monitors for user prefs changes.
  std::unique_ptr<PrefChangeRegistrar> user_prefs_registrar_;

  base::WeakPtrFactory<AccessCodeCastSinkService> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_
