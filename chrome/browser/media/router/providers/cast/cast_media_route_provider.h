// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_channel {
class CastMessageHandler;
}

namespace url {
class Origin;
}

namespace media_router {

class CastActivityManager;
class CastSessionTracker;

// MediaRouteProvider for Cast sinks. This class may be created on any sequence.
// All other methods, however, must be called on the task runner provided
// during construction.
class CastMediaRouteProvider : public mojom::MediaRouteProvider {
 public:
  CastMediaRouteProvider(
      mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
      mojo::PendingRemote<mojom::MediaRouter> media_router,
      MediaSinkServiceBase* media_sink_service,
      CastAppDiscoveryService* app_discovery_service,
      cast_channel::CastMessageHandler* message_handler,
      const std::string& hash_token,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  CastMediaRouteProvider(const CastMediaRouteProvider&) = delete;
  CastMediaRouteProvider& operator=(const CastMediaRouteProvider&) = delete;

  ~CastMediaRouteProvider() override;

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

  CastActivityManager* GetCastActivityManagerForTest() {
    return activity_manager_.get();
  }

 private:
  friend class CastMediaRouteProviderTest;
  FRIEND_TEST_ALL_PREFIXES(CastMediaRouteProviderTest,
                           GetRemotePlaybackCompatibleSinks);

  void Init(mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
            mojo::PendingRemote<mojom::MediaRouter> media_router,
            CastSessionTracker* session_tracker,
            const std::string& hash_token);

  // Notifies |media_router_| that results for a sink query has been updated.
  void OnSinkQueryUpdated(const MediaSource::Id& source_id,
                          const std::vector<MediaSinkInternal>& sinks);

  // Binds |this| to the Mojo receiver passed into the ctor.
  mojo::Receiver<mojom::MediaRouteProvider> receiver_{this};

  // Mojo remote to the Media Router.
  mojo::Remote<mojom::MediaRouter> media_router_;

  // Mojo remote to the logger owned by the Media Router.
  mojo::Remote<mojom::Logger> logger_;

  // Non-owned pointer to the Cast MediaSinkServiceBase instance.
  const raw_ptr<MediaSinkServiceBase> media_sink_service_;

  // Non-owned pointer to the CastAppDiscoveryService instance.
  const raw_ptr<CastAppDiscoveryService> app_discovery_service_;

  // Non-owned pointer to the CastMessageHandler instance.
  const raw_ptr<cast_channel::CastMessageHandler> message_handler_;

  // Registered sink queries.
  base::flat_map<MediaSource::Id, base::CallbackListSubscription> sink_queries_;

  std::unique_ptr<CastActivityManager> activity_manager_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_ROUTE_PROVIDER_H_
