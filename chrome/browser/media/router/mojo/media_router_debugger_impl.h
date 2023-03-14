// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DEBUGGER_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DEBUGGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/browser/media_routes_observer.h"

namespace media_router {

// An implementation for media router debugging and feedback.
class MediaRouterDebuggerImpl : public MediaRouterDebugger,
                                public MediaRoutesObserver {
 public:
  explicit MediaRouterDebuggerImpl(MediaRouterMojoImpl& router);

  MediaRouterDebuggerImpl(const MediaRouterDebuggerImpl&) = delete;
  MediaRouterDebuggerImpl& operator=(const MediaRouterDebuggerImpl&) = delete;

  ~MediaRouterDebuggerImpl() override;

 protected:
  friend class MediaRouterDebuggerImplTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDebuggerImplTest, ReportsNotEnabled);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDebuggerImplTest, NonMirroringRoutes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDebuggerImplTest, FetchMirroringStats);

  void NotifyGetMirroringStats(const base::Value::Dict& json_logs);

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override;

  void OnMirroringRouteAdded(const MediaRoute::Id& route_id);
  void OnMirroringRouteRemoved();

  void ScheduleFetchMirroringStats(
      const base::TimeDelta& init_delay = base::Seconds(0));
  void FetchMirroringStats();

  void OnStatsFetched(const base::Value json_stats_cb);

  // Set of route ids that is updated whenever OnRoutesUpdated is called. We
  // store this value to check whether a route was removed or not.
  std::vector<MediaRoute::Id> previous_routes_;

  // The last route mirroring route was added via the MediaRoutesObserver. If
  // more than one mirroring route is added, the last added route is chosen.
  absl::optional<MediaRoute::Id> current_mirroring_route_id_;

  media_router::MediaRouterMojoImpl& router_;

  base::WeakPtrFactory<MediaRouterDebuggerImpl> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DEBUGGER_IMPL_H_
