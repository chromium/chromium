// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TIPS_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TIPS_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "components/prefs/pref_service.h"

namespace notifications {

class TipsAgent;

// The client used in Clank Tips and chrome://notifications-internals for testing.
class TipsClient : public NotificationSchedulerClient {
 public:
  explicit TipsClient(std::unique_ptr<TipsAgent> tips_agent,
                      PrefService* pref_service);
  TipsClient(const TipsClient&) = delete;
  TipsClient& operator=(const TipsClient&) = delete;
  ~TipsClient() override;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const UserActionData& action_data) override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;

  std::unique_ptr<TipsAgent> tips_agent_;
  // The pointer for this PrefService is susceptible to becoming a dangling
  // pointer during testing in notification_schedule_service_browsertest.cc's
  // Show/ScheduleNotification, when passed in to the schedule service. This is
  // safe in practice because the schedule service (including the tips client)
  // as part of a keyed service will be cleaned up after it is used before
  // PrefService is cleaned up since there is an explicit dependency.
  raw_ptr<PrefService, DisableDanglingPtrDetection> pref_service_;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TIPS_CLIENT_H_
