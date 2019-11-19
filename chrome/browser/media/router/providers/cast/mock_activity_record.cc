// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mock_activity_record.h"

namespace media_router {

MockActivityRecord::MockActivityRecord(const MediaRoute& route,
                                       const std::string& app_id)
    : ActivityRecord(route, app_id, nullptr, nullptr) {}

MockActivityRecord::~MockActivityRecord() = default;

}  // namespace media_router
