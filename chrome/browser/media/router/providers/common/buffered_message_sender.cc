// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/common/buffered_message_sender.h"

namespace media_router {

BufferedMessageSender::BufferedMessageSender(mojom::MediaRouter* media_router)
    : media_router_(media_router) {
  DCHECK(media_router_);
}

BufferedMessageSender::~BufferedMessageSender() = default;

void BufferedMessageSender::SendMessages(
    const MediaRoute::Id& route_id,
    std::vector<mojom::RouteMessagePtr> messages) {
  if (base::Contains(active_routes_, route_id)) {
    media_router_->OnRouteMessagesReceived(route_id, std::move(messages));
  } else {
    auto& buffer = buffered_messages_[route_id];
    for (auto& message : messages)
      buffer.emplace_back(std::move(message));
  }
}

void BufferedMessageSender::StartListening(const MediaRoute::Id& route_id) {
  active_routes_.insert(route_id);
  auto buffer_it = buffered_messages_.find(route_id);
  if (buffer_it != buffered_messages_.end()) {
    media_router_->OnRouteMessagesReceived(route_id,
                                           std::move(buffer_it->second));
    buffered_messages_.erase(buffer_it);
  }
}

void BufferedMessageSender::StopListening(const MediaRoute::Id& route_id) {
  active_routes_.erase(route_id);
  buffered_messages_.erase(route_id);
}

}  // namespace media_router
