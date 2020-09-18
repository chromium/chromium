// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_client.h"

namespace media_router {

CastSessionClient::CastSessionClient(const std::string& client_id,
                                     const url::Origin& origin,
                                     int tab_id)
    : client_id_(client_id), origin_(origin), tab_id_(tab_id) {}

CastSessionClient::~CastSessionClient() = default;

}  // namespace media_router
