// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_IMPL_H_

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"

namespace notifications {
class NotificationScheduleService;
struct ClientOverview;
}  // namespace notifications

namespace offline_pages {
namespace prefetch {

class PrefetchNotificationServiceBridge;

// Service to manage offline prefetch notifications via
// notifications::NotificationScheduleService.
class PrefetchNotificationServiceImpl : public PrefetchNotificationService {
 public:
  PrefetchNotificationServiceImpl(
      notifications::NotificationScheduleService* schedule_service,
      std::unique_ptr<PrefetchNotificationServiceBridge> bridge,
      base::Clock* clock);

  ~PrefetchNotificationServiceImpl() override;

 private:
  // PrefetchNotificationService implementation.
  void Schedule(const base::string16& title,
                const base::string16& body) override;
  void OnClick() override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;

  // Called after querying the client status, and execute schedule actual task.
  void ScheduleInternal(const base::string16& title,
                        const base::string16& body,
                        notifications::ClientOverview);
  // Called when client_overview is queried, and determine the custom throttle
  // config.
  void OnClientOverviewQueried(ThrottleConfigCallback callback,
                               notifications::ClientOverview client_overview);

  // Used to schedule notification to show in the future. Must outlive this
  // class.
  notifications::NotificationScheduleService* schedule_service_;

  std::unique_ptr<PrefetchNotificationServiceBridge> bridge_;

  base::Clock* clock_;

  base::WeakPtrFactory<PrefetchNotificationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace prefetch
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_IMPL_H_
