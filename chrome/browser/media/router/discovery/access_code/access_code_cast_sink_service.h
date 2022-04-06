// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
#include "net/base/backoff_entry.h"

namespace media_router {

using ChannelOpenedCallback = base::OnceCallback<void(bool)>;
using AddSinkResultCode = access_code_cast::mojom::AddSinkResultCode;

class AccessCodeCastSinkService : public KeyedService,
                                  public DiscoveryNetworkMonitor::Observer {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  using AddSinkResultCallback = base::OnceCallback<void(
      access_code_cast::mojom::AddSinkResultCode add_sink_result,
      absl::optional<MediaSink::Id> sink_id)>;

  AccessCodeCastSinkService(const AccessCodeCastSinkService&) = delete;
  AccessCodeCastSinkService& operator=(const AccessCodeCastSinkService&) =
      delete;

  ~AccessCodeCastSinkService() override;

  base::WeakPtr<AccessCodeCastSinkService> GetWeakPtr();

  // With the given |access_code|, make a call to the discovery server to
  // validate the device.
  // |access_code|: the access code that is sent to the discovery server.
  // |callback|: a callback sent that returns a discovery device and the status
  // of that device.
  void DiscoverSink(const std::string& access_code,
                    AddSinkResultCallback callback);

  // Attempts to add a sink to the Media Router.
  // |sink|: the sink that is added to the router.
  // |callback|: a callback that tracks the status of opening a cast channel to
  // the given media sink.
  virtual void AddSinkToMediaRouter(const MediaSinkInternal& sink,
                                    AddSinkResultCallback add_sink_callback);

  void StoreSinkInPrefs(const MediaSinkInternal* sink);
  void StoreSinkInPrefsById(const MediaSink::Id sink_id);

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

   private:
    FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                             AccessCodeCastDeviceRemovedAfterRouteEnds);
    FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                             AddExistingSinkToMediaRouterWithRoute);
    // media_router::MediaRoutesObserver:
    void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

    // Set of route ids that is updated whenever OnRoutesUpdated is called.
    std::vector<MediaRoute::Id> old_routes_;

    MediaRoute::Id removed_route_id_;

    const raw_ptr<AccessCodeCastSinkService> access_code_sink_service_;

    base::WeakPtrFactory<AccessCodeMediaRoutesObserver> weak_ptr_factory_{this};
  };
  friend class AccessCodeCastSinkServiceFactory;
  friend class AccessCodeCastSinkServiceTest;
  friend class AccessCodeCastHandlerTest;
  friend class MockAccessCodeCastSinkService;
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           AccessCodeCastDeviceRemovedAfterRouteEnds);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           AddExistingSinkToMediaRouter);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           AddNewSinkToMediaRouter);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           AddExistingSinkToMediaRouterWithRoute);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           DiscoveryDeviceMissingWithOk);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           ValidDiscoveryDeviceAndCode);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           InvalidDiscoveryDevice);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest, NonOKResultCode);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           OnChannelOpenedSuccess);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           OnChannelOpenedFailure);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           SinkDoesntExistForPrefs);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           TestFetchAndAddStoredDevices);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest, TestChangeNetworks);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           TestAddInvalidDevicesNoMediaSinkInternal);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastSinkServiceTest,
                           TestFetchAndAddStoredDevicesNoNetwork);

  // Constructor used for testing.
  AccessCodeCastSinkService(
      Profile* profile,
      MediaRouter* media_router,
      CastMediaSinkServiceImpl* cast_media_sink_service_impl,
      DiscoveryNetworkMonitor* network_monitor,
      PrefService* prefs);

  // Use |AccessCodeCastSinkServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit AccessCodeCastSinkService(Profile* profile);

  void OnAccessCodeValidated(AddSinkResultCallback add_sink_callback,
                             absl::optional<DiscoveryDevice> discovery_device,
                             AddSinkResultCode result_code);

  void OnChannelOpenedResult(AddSinkResultCallback add_sink_callback,
                             MediaSink::Id sink_id,
                             bool channel_opened);

  void HandleMediaRouteDiscoveredByAccessCode(const MediaSinkInternal* sink);
  void OnAccessCodeRouteRemoved(const MediaSinkInternal& sink);
  void OpenChannelIfNecessary(const MediaSinkInternal& sink,
                              AddSinkResultCallback add_sink_callback,
                              bool has_sink);

  void InitStoredDeviceConnections();
  void FetchAndAddStoredDevices(const std::string& network_id);
  void AddStoredDevicesToMediaRouter(const base::Value::List& sink_ids);

  // Removes the given |sink_id| from all entries in the AccessCodeCast pref
  // service.
  void RemoveSinkIdFromAllEntries(const MediaSink::Id& sink_id);

  // Validates the given |sink_id| is present and properly stored as a
  // MediaSinkInternal in the pref store. If the sink is present, a
  // MediaSinkInternal will be returned in the optional value.
  const absl::optional<MediaSinkInternal> ValidateDeviceFromSinkId(
      const MediaSink::Id& sink_id);

  // DiscoveryNetworkMonitor::Observer implementation
  void OnNetworksChanged(const std::string& network_id) override;

  cast_channel::CastSocketOpenParams CreateCastSocketOpenParams(
      const MediaSinkInternal& sink);

  // KeyedService.
  void Shutdown() override;

  void SetTaskRunnerForTest(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    task_runner_ = task_runner;
  }

  // Owns us via the KeyedService mechanism.
  const raw_ptr<Profile> profile_;

  const raw_ptr<media_router::MediaRouter> media_router_;

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

  // Map of callbacks that we are currently waiting to alert callers about the
  // completion of discovery. This cannot be done until all routes on any given
  // sink are terminated.
  std::map<MediaSink::Id, AddSinkResultCallback> pending_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Raw pointer to DiscoveryNetworkMonitor, which is a global leaky singleton
  // and manages network change notifications.
  const raw_ptr<DiscoveryNetworkMonitor> network_monitor_;

  std::unique_ptr<AccessCodeCastPrefUpdater> pref_updater_;

  base::WeakPtrFactory<AccessCodeCastSinkService> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_
