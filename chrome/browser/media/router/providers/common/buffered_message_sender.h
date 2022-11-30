// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_COMMON_BUFFERED_MESSAGE_SENDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_COMMON_BUFFERED_MESSAGE_SENDER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace media_router {

// Used by MediaRouteProviders to buffer outgoing route messages until they
// are ready to be sent.
// TODO(crbug.com/1154482): to be replaced by PresentationConnection mojom
// message pipes which come with their own message buffering.
class BufferedMessageSender {
 public:
  explicit BufferedMessageSender(mojom::MediaRouter* media_router);

  BufferedMessageSender(const BufferedMessageSender&) = delete;
  BufferedMessageSender& operator=(const BufferedMessageSender&) = delete;

  ~BufferedMessageSender();

  // Sends |messages| for route given by |route_id|. The messages are buffered
  // if there are no listeners for the route. It is invalid to call this method
  // for an already terminated route.
  void SendMessages(const MediaRoute::Id& route_id,
                    std::vector<mojom::RouteMessagePtr> messages);

  // Starts listening for messages for |route_id|. All previously buffered
  // messages and subsequent messages will be sent to |media_router_|
  void StartListening(const MediaRoute::Id& route_id);

  // Stops listening for messages for |route_id|. Any buffered messages will be
  // discarded.
  void StopListening(const MediaRoute::Id& route_id);

 private:
  // Set of MediaRoutes for which there is an active message listener.
  base::flat_set<MediaRoute::Id> active_routes_;
  base::flat_map<MediaRoute::Id, std::vector<mojom::RouteMessagePtr>>
      buffered_messages_;

  // Non-owned pointer provided by DialMediaRouteProvider.
  const raw_ptr<mojom::MediaRouter> media_router_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_COMMON_BUFFERED_MESSAGE_SENDER_H_
