// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_NOTIFICATION_H_

#include <string>

#include "ash/multi_capture/multi_capture_service_client.h"
#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"

namespace url {
class Origin;
}  // namespace url

namespace ash {

// MultiCaptureNotification manages the notification informing the user of
// automatic multi captures being started. On managed devices, administrators
// can enforce automatic capturing by using the getDisplayMediaSet API.
// Users are notified to make sure their privacy is respected.
class MultiCaptureNotification : public MultiCaptureServiceClient::Observer {
 public:
  MultiCaptureNotification();

  MultiCaptureNotification(const MultiCaptureNotification&) = delete;
  MultiCaptureNotification& operator=(const MultiCaptureNotification&) = delete;

  ~MultiCaptureNotification() override;

  // MultiCaptureServiceClient::Observer:
  void MultiCaptureStarted(const std::string& label,
                           const url::Origin& origin) override;
  void MultiCaptureStopped(const std::string& label) override;
  void MultiCaptureServiceClientDestroyed() override;

 private:
  // Maps the multi capture label (as received in `MultiCaptureStarted` and
  // `MultiCaptureStopped`) to the notification id.
  base::flat_map<std::string, std::string> notification_ids_;

  base::ScopedObservation<MultiCaptureServiceClient,
                          MultiCaptureServiceClient::Observer>
      multi_capture_service_client_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_LOW_DISK_NOTIFICATION_H_
