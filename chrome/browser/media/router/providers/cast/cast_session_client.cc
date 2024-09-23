// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_client.h"

namespace media_router {

CastSessionClient::CastSessionClient(
    const std::string& client_id,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id)
    : client_id_(client_id),
      origin_(origin),
      frame_tree_node_id_(frame_tree_node_id) {}

CastSessionClient::~CastSessionClient() = default;

}  // namespace media_router
