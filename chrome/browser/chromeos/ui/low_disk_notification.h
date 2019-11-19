// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_LOW_DISK_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_UI_LOW_DISK_NOTIFICATION_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"

namespace message_center {
class Notification;
}

namespace chromeos {

// LowDiskNotification manages the notification informing the user of low disk
// space.  It is responsible for both creating the notifications in message
// center as well as throttling notifications shown to the user.
// This class should be created after DBus has been initialized and destroyed
// before DBus has been shutdown.
// This class must be instantiated on the UI thread.
class LowDiskNotification : public CryptohomeClient::Observer {
 public:
  // Registers this class as the CryptohomeClient LowDiskSpaceHandler.
  LowDiskNotification();

  // Resets CryptohomeClient LowDiskSpaceHandler.
  ~LowDiskNotification() override;

  // Called when the device is running low on disk space.  This is responsible
  // for deciding whether a notification should be shown or not and showing it
  // if appropriate.  This must be called from the thread that instantiated this
  // object.
  void LowDiskSpace(uint64_t free_disk_bytes) override;

 private:
  friend class LowDiskNotificationTest;

  enum Severity { NONE = -1, MEDIUM = 0, HIGH = 1 };

  // Creates a notification for the specified severity.  If the severity does
  // not match a known value MEDIUM is used by default.
  std::unique_ptr<message_center::Notification> CreateNotification(
      Severity severity);

  // Gets the severity of the low disk status based on the amount of free space
  // left on the disk.
  Severity GetSeverity(uint64_t free_disk_bytes);

  // Sets the minimum time to wait between notifications of the same severity.
  // Should only be used in tests.
  void SetNotificationIntervalForTest(base::TimeDelta interval);

  base::Time last_notification_time_;
  Severity last_notification_severity_ = NONE;
  base::TimeDelta notification_interval_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<LowDiskNotification> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LowDiskNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_LOW_DISK_NOTIFICATION_H_
