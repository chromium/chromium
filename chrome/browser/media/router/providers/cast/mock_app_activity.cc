// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mock_app_activity.h"

namespace media_router {

MockAppActivity::MockAppActivity(const MediaRoute& route,
                                 const std::string& app_id)
    : AppActivity(route, app_id, nullptr, nullptr) {}

MockAppActivity::~MockAppActivity() = default;

}  // namespace media_router
