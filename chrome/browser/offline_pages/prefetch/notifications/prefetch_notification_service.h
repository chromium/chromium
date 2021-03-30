// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace notifications {
struct ThrottleConfig;
}

namespace offline_pages {
namespace prefetch {

// Service to manage offline prefetch notifications via
// notifications::NotificationScheduleService.
class PrefetchNotificationService : public KeyedService {
 public:
  using ThrottleConfigCallback =
      base::OnceCallback<void(std::unique_ptr<notifications::ThrottleConfig>)>;

  // Schedules an prefetch notification.
  virtual void Schedule(const std::u16string& title,
                        const std::u16string& body) = 0;

  // Called when the notification is clicked by the user.
  virtual void OnClick() = 0;

  // Gives customized throttle config.
  virtual void GetThrottleConfig(ThrottleConfigCallback callback) = 0;

  ~PrefetchNotificationService() override = default;

 protected:
  PrefetchNotificationService() = default;
};

}  // namespace prefetch
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_H_
