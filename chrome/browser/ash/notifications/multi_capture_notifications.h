// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_NOTIFICATIONS_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_NOTIFICATIONS_H_

#include <map>
#include <memory>
#include <string>

#include "ash/multi_capture/multi_capture_service_client.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/login_state/login_state.h"

namespace url {
class Origin;
}  // namespace url

namespace ash {

// MultiCaptureNotification manages the notification informing the user of
// automatic multi captures being started. On managed devices, administrators
// can enforce automatic capturing by using the getAllScreensMedia API.
// Users are notified to make sure their privacy is respected.
class MultiCaptureNotifications : public MultiCaptureServiceClient::Observer,
                                  public LoginState::Observer {
 public:
  struct NotificationMetadata {
    NotificationMetadata(std::string id, base::TimeTicks time_created);
    NotificationMetadata(NotificationMetadata&& metadata);
    NotificationMetadata& operator=(NotificationMetadata&& metadata);
    NotificationMetadata(const NotificationMetadata& metadata) = delete;
    NotificationMetadata& operator=(NotificationMetadata other) = delete;
    ~NotificationMetadata();

    // `id` is used as ID for the notification in the `SystemNotificationHelper`
    // creation.
    std::string id;
    // `time_created` is used to compute for how long the notification is
    // already shown.
    base::TimeTicks time_created;
    // `closing_timer` is used to make sure that the notification remains
    // visible for at least six seconds.
    std::unique_ptr<base::OneShotTimer> closing_timer;
  };

  MultiCaptureNotifications();

  MultiCaptureNotifications(const MultiCaptureNotifications&) = delete;
  MultiCaptureNotifications& operator=(const MultiCaptureNotifications&) =
      delete;

  ~MultiCaptureNotifications() override;

  // MultiCaptureServiceClient::Observer:
  void MultiCaptureStarted(const std::string& label,
                           const url::Origin& origin) override;
  void MultiCaptureStartedFromApp(const std::string& label,
                                  const std::string& app_id,
                                  const std::string& app_short_name) override;
  void MultiCaptureStopped(const std::string& label) override;
  void MultiCaptureServiceClientDestroyed() override;

  // LoginState::Observer:
  void LoggedInStateChanged() override;

 private:
  void MultiCaptureStartedInternal(const std::string& label,
                                   const std::string& notification_id,
                                   const std::string& app_name);

  // Maps the multi capture label (as received in `MultiCaptureStarted` and
  // `MultiCaptureStopped`) to the notification metadata.
  std::map<std::string, NotificationMetadata> notifications_metadata_;

  base::ScopedObservation<MultiCaptureServiceClient,
                          MultiCaptureServiceClient::Observer>
      multi_capture_service_client_observation_{this};
  base::ScopedObservation<LoginState, LoginState::Observer>
      login_state_observation_{this};
  base::WeakPtrFactory<MultiCaptureNotifications> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_NOTIFICATIONS_H_
