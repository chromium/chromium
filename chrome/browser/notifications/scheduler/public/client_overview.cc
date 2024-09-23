// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/client_overview.h"

#include <utility>

namespace notifications {

ClientOverview::ClientOverview() : num_scheduled_notifications(0) {}

ClientOverview::ClientOverview(ImpressionDetail impression_detail,
                               size_t num_scheduled_notifications)
    : impression_detail(std::move(impression_detail)),
      num_scheduled_notifications(num_scheduled_notifications) {}

ClientOverview::ClientOverview(const ClientOverview& other) = default;

ClientOverview::ClientOverview(ClientOverview&& other) = default;

ClientOverview& ClientOverview::operator=(const ClientOverview& other) =
    default;

ClientOverview& ClientOverview::operator=(ClientOverview&& other) = default;

ClientOverview::~ClientOverview() = default;

bool ClientOverview::operator==(const ClientOverview& other) const {
  return num_scheduled_notifications == other.num_scheduled_notifications &&
         impression_detail == other.impression_detail;
}

}  // namespace notifications
