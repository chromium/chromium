// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ROUTE_MESSAGE_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ROUTE_MESSAGE_OBSERVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"

namespace media_router {

class MediaRouter;

// Observes messages originating from the MediaSink connected to a MediaRoute.
// Messages are received from MediaRouter via |OnMessagesReceived|.
// TODO(imcheng): Rename to PresentationConnectionMessageObserver.
class RouteMessageObserver {
 public:
  // |route_id|: ID of MediaRoute to listen for messages.
  RouteMessageObserver(MediaRouter* router, const MediaRoute::Id& route_id);

  virtual ~RouteMessageObserver();

  // Invoked by |router_| whenever messages are received from the route sink.
  // |messages| is guaranteed to be non-empty.
  virtual void OnMessagesReceived(
      std::vector<mojom::RouteMessagePtr> messages) = 0;

  const MediaRoute::Id& route_id() const { return route_id_; }

 private:
  MediaRouter* const router_;
  const MediaRoute::Id route_id_;

  DISALLOW_COPY_AND_ASSIGN(RouteMessageObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ROUTE_MESSAGE_OBSERVER_H_
