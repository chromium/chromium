// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_REGISTRAR_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_REGISTRAR_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

class NotificationSchedulerClient;

// Registers and maintains a list of NotificationSchedulerClient
// implementations.
class NotificationSchedulerClientRegistrar {
 public:
  NotificationSchedulerClientRegistrar();
  NotificationSchedulerClientRegistrar(
      const NotificationSchedulerClientRegistrar&) = delete;
  NotificationSchedulerClientRegistrar& operator=(
      const NotificationSchedulerClientRegistrar&) = delete;
  ~NotificationSchedulerClientRegistrar();

  // Registers a client into notification scheduler system.
  void RegisterClient(SchedulerClientType type,
                      std::unique_ptr<NotificationSchedulerClient> client);

  // Gets a NotificationSchedulerClient, nullptr if the type doesn't exist.
  NotificationSchedulerClient* GetClient(SchedulerClientType type);

  // Gets a list of registered clients, sorted by integer value of
  // SchedulerClientType.
  void GetRegisteredClients(std::vector<SchedulerClientType>* clients) const;

 private:
  using ClientsMap = std::map<SchedulerClientType,
                              std::unique_ptr<NotificationSchedulerClient>>;
  ClientsMap clients_;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_REGISTRAR_H_
