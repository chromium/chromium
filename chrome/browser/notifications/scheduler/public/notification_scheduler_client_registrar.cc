// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace notifications {

NotificationSchedulerClientRegistrar::NotificationSchedulerClientRegistrar() =
    default;

NotificationSchedulerClientRegistrar::~NotificationSchedulerClientRegistrar() =
    default;

void NotificationSchedulerClientRegistrar::RegisterClient(
    SchedulerClientType type,
    std::unique_ptr<NotificationSchedulerClient> client) {
  DCHECK(clients_.find(type) == clients_.end());
  clients_.emplace(type, std::move(client));
}

NotificationSchedulerClient* NotificationSchedulerClientRegistrar::GetClient(
    SchedulerClientType type) {
  auto it = clients_.find(type);
  if (it == clients_.end())
    return nullptr;
  return it->second.get();
}

void NotificationSchedulerClientRegistrar::GetRegisteredClients(
    std::vector<SchedulerClientType>* clients) const {
  DCHECK(clients);
  clients->clear();
  for (const auto& pair : clients_) {
    clients->emplace_back(pair.first);
  }
}

}  // namespace notifications
