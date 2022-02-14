// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "net/base/backoff_entry.h"

namespace media_router {

using ChannelOpenedCallback = base::OnceCallback<void(bool)>;

class AccessCodeCastSinkService : public KeyedService {
 public:
  AccessCodeCastSinkService(const AccessCodeCastSinkService&) = delete;
  AccessCodeCastSinkService& operator=(const AccessCodeCastSinkService&) =
      delete;

  ~AccessCodeCastSinkService() override;

  base::WeakPtr<AccessCodeCastSinkService> GetWeakPtr();

  // Attempts to add a sink to the Media Router.
  // |sink|: the sink that is added to the router.
  // |callback|: a callback that tracks the status of opening a cast channel to
  // the given media sink.
  virtual void AddSinkToMediaRouter(const MediaSinkInternal& sink,
                                    ChannelOpenedCallback callback);

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
    // media_router::MediaRoutesObserver:
    void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

    // Set of route ids that is updated whenever OnRoutesUpdated is called.
    std::vector<MediaRoute::Id> old_routes_;

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

  // Constructor used for testing.
  AccessCodeCastSinkService(
      Profile* profile,
      MediaRouter* media_router,
      CastMediaSinkServiceImpl* cast_media_sink_service_impl);

  // Use |AccessCodeCastSinkServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit AccessCodeCastSinkService(Profile* profile);

  void HandleMediaRouteDiscoveredByAccessCode(const MediaSinkInternal* sink);
  void OnAccessCodeRouteRemoved(const MediaSinkInternal& sink);
  void OpenChannelIfNecessary(const MediaSinkInternal& sink,
                              ChannelOpenedCallback callback,
                              bool has_sink);

  // KeyedService.
  void Shutdown() override;

  // Owns us via the KeyedService mechanism.
  const raw_ptr<Profile> profile_;

  // Helper class for observing the removal of MediaRoutes.
  std::unique_ptr<AccessCodeMediaRoutesObserver> media_routes_observer_;

  // Raw pointer of leaky singleton CastMediaSinkServiceImpl, which manages
  // the addition and removal of cast sinks in the Media Router. This is
  // guaranteed to be destroyed after destruction of the
  // AccessCodeCastSinkService.
  const raw_ptr<media_router::CastMediaSinkServiceImpl>
      cast_media_sink_service_impl_;

  net::BackoffEntry::Policy backoff_policy_;

  base::WeakPtrFactory<AccessCodeCastSinkService> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_H_
