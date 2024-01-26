// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_DEBUGD_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_DEBUGD_NOTIFICATION_HANDLER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

// Controller class to handle debug_daemon_client's notifications.
class DebugdNotificationHandler : public DebugDaemonClient::Observer {
 public:
  explicit DebugdNotificationHandler(DebugDaemonClient* debug_daemon_client);
  ~DebugdNotificationHandler() override;
  DebugdNotificationHandler(const DebugdNotificationHandler&) = delete;
  DebugdNotificationHandler& operator=(const DebugdNotificationHandler&) =
      delete;

  // DebugDaemonClient::Observer
  void OnPacketCaptureStarted() override;
  void OnPacketCaptureStopped() override;

 private:
  std::unique_ptr<message_center::Notification> CreateNotification();
  void CloseNotification();
  void OnButtonClick(std::optional<int> /* button_idx */);

  const raw_ptr<DebugDaemonClient> debug_daemon_client_;
  base::WeakPtrFactory<DebugdNotificationHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_DEBUGD_NOTIFICATION_HANDLER_H_
