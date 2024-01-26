// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"
#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace url {
class Origin;
}

namespace media_router {

// MediaRouteProvider for DIAL sinks.
// DialMediaRouteProvider supports custom DIAL launch, which is a
// way for websites that uses Cast SDK to launch apps on DIAL devices.
// The life of a custom DIAL launch workflow is as follows:
// 1) The user initiates a custom DIAL launch by selecting a DIAL device
//    to cast to. This becomes a CreateRoute request to the
//    DialMediaRouteProvider.
// 2) The DialMediaRouteProvider sends NEW_SESSION / RECEIVER_ACTION messages to
//    the Cast SDK to inform of a new Cast session. In addition, a
//    CUSTOM_DIAL_LAUNCH request is sent to the Cast SDK.
// 3) The Cast SDK sends back a CUSTOM_DIAL_LAUNCH response. Depending on the
//    response, either the page have already handled the app launch, or the
//    DialMediaRouteProvider will initiate the app launch on the device.
// 4) Once the app is launched, the workflow is complete. The webpage will then
//    communicate with the app on the device via its own mechanism.
class DialMediaRouteProvider : public mojom::MediaRouteProvider,
                               public MediaSinkServiceBase::Observer {
 public:
  // |receiver|: Mojo receiver to bind to |this|.
  // |media_router|: Pending remote to MediaRouter.
  // |media_sink_service|: DIAL MediaSinkService providing information on sinks.
  // |hash_token|: A per-profile value used to hash sink IDs.
  // |task_runner|: The task runner on which |this| runs.
  DialMediaRouteProvider(
      mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
      mojo::PendingRemote<mojom::MediaRouter> media_router,
      DialMediaSinkServiceImpl* media_sink_service,
      const std::string& hash_token,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  DialMediaRouteProvider(const DialMediaRouteProvider&) = delete;
  DialMediaRouteProvider& operator=(const DialMediaRouteProvider&) = delete;

  ~DialMediaRouteProvider() override;

  // mojom::MediaRouteProvider:
  void CreateRoute(const std::string& media_source,
                   const std::string& sink_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int32_t frame_tree_node_id,
                   base::TimeDelta timeout,
                   CreateRouteCallback callback) override;
  void JoinRoute(const std::string& media_source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 int32_t frame_tree_node_id,
                 base::TimeDelta timeout,
                 JoinRouteCallback callback) override;
  void TerminateRoute(const std::string& route_id,
                      TerminateRouteCallback callback) override;
  void SendRouteMessage(const std::string& media_route_id,
                        const std::string& message) override;
  void SendRouteBinaryMessage(const std::string& media_route_id,
                              const std::vector<uint8_t>& data) override;
  void StartObservingMediaSinks(const std::string& media_source) override;
  void StopObservingMediaSinks(const std::string& media_source) override;
  void StartObservingMediaRoutes() override;
  void DetachRoute(const std::string& route_id) override;
  void EnableMdnsDiscovery() override;
  void DiscoverSinksNow() override;
  void BindMediaController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer,
      BindMediaControllerCallback callback) override;
  void GetState(GetStateCallback callback) override;

  void SetActivityManagerForTest(
      std::unique_ptr<DialActivityManager> activity_manager);

 private:
  FRIEND_TEST_ALL_PREFIXES(DialMediaRouteProviderTest, AddRemoveSinkQuery);
  FRIEND_TEST_ALL_PREFIXES(DialMediaRouteProviderTest,
                           AddSinkQuerySameMediaSource);
  FRIEND_TEST_ALL_PREFIXES(DialMediaRouteProviderTest,
                           AddSinkQuerySameAppDifferentMediaSources);
  FRIEND_TEST_ALL_PREFIXES(DialMediaRouteProviderTest,
                           AddSinkQueryDifferentApps);
  FRIEND_TEST_ALL_PREFIXES(DialMediaRouteProviderTest, ListenForRouteMessages);

  struct MediaSinkQuery {
    MediaSinkQuery();

    MediaSinkQuery(const MediaSinkQuery&) = delete;
    MediaSinkQuery& operator=(const MediaSinkQuery&) = delete;

    ~MediaSinkQuery();

    // Set of registered media sources for current sink query.
    base::flat_set<MediaSource> media_sources;
    base::CallbackListSubscription subscription;
  };

  // MediaSinkServiceBase::Observer:
  void OnSinksDiscovered(const std::vector<MediaSinkInternal>& sinks) override;

  // Binds the message pipes |receiver| and |media_router| to |this|.
  void Init(mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
            mojo::PendingRemote<mojom::MediaRouter> media_router);

  void OnAvailableSinksUpdated(const std::string& app_name);

  void NotifyOnSinksReceived(const MediaSource::Id& source_id,
                             const std::vector<MediaSinkInternal>& sinks,
                             const std::vector<url::Origin>& origins);

  void HandleParsedRouteMessage(const MediaRoute::Id& route_id,
                                data_decoder::DataDecoder::ValueOrError result);
  void HandleClientConnect(const DialActivity& activity,
                           const MediaSinkInternal& sink);
  void SendCustomDialLaunchMessage(const MediaRoute::Id& route_id,
                                   const MediaSink::Id& sink_id,
                                   const std::string& app_name,
                                   DialAppInfoResult result);
  void SendDialAppInfoResponse(const MediaRoute::Id& route_id,
                               int sequence_number,
                               const MediaSink::Id& sink_id,
                               const std::string& app_name,
                               DialAppInfoResult result);
  void HandleCustomDialLaunchResponse(const DialActivity& activity,
                                      const DialInternalMessage& message);
  void HandleDiapAppInfoRequest(const DialActivity& activity,
                                const DialInternalMessage& message,
                                const MediaSinkInternal& sink);
  void HandleAppLaunchResult(const MediaRoute::Id& route_id, bool success);
  void DoTerminateRoute(const DialActivity& activity,
                        const MediaSinkInternal& sink,
                        TerminateRouteCallback callback);
  void HandleStopAppResult(const MediaRoute::Id& route_id,
                           TerminateRouteCallback callback,
                           const std::optional<std::string>& message,
                           mojom::RouteRequestResultCode result_code);
  void NotifyAllOnRoutesUpdated();
  void NotifyOnRoutesUpdated(const std::vector<MediaRoute>& routes);

  // Returns a list of valid origins for |app_name|. Returns an empty list if
  // all origins are valid.
  std::vector<url::Origin> GetOrigins(const std::string& app_name);

  // Binds |this| to the Mojo receiver passed into the ctor.
  mojo::Receiver<mojom::MediaRouteProvider> receiver_{this};

  // Mojo remote to the Media Router.
  mojo::Remote<mojom::MediaRouter> media_router_;

  // Non-owned pointer to the DialMediaSinkServiceImpl instance.
  const raw_ptr<DialMediaSinkServiceImpl> media_sink_service_;

  // Map of media sink queries, keyed by app name.
  base::flat_map<std::string, std::unique_ptr<MediaSinkQuery>>
      media_sink_queries_;

  // Set of route queries by MediaSource ID.
  base::flat_set<MediaSource::Id> media_route_queries_;

  // Set of pending DIAL launches by sequence number. The max number of pending
  // launches is capped, and oldest entries (smallest number) will be evicted.
  base::flat_set<int> pending_dial_launches_;

  std::unique_ptr<DialActivityManager> activity_manager_;

  DialInternalMessageUtil internal_message_util_;

  // Mojo remote to the logger owned by the Media Router.
  mojo::Remote<mojom::Logger> logger_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DialMediaRouteProvider> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_H_
