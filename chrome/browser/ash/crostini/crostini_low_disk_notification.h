// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_LOW_DISK_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_LOW_DISK_NOTIFICATION_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"

namespace message_center {
class Notification;
}

namespace crostini {

// CrostiniLowDiskNotification manages the notification informing the user of
// low disk space. It is responsible for both creating the notifications in
// message center as well as throttling notifications shown to the user. This
// class should be created after DBus has been initialized and destroyed before
// DBus has been shutdown.
// This class must be instantiated on the UI thread.
class CrostiniLowDiskNotification : public ash::CiceroneClient::Observer {
 public:
  // Registers this class as a Cicerone Observer.
  CrostiniLowDiskNotification();

  CrostiniLowDiskNotification(const CrostiniLowDiskNotification&) = delete;
  CrostiniLowDiskNotification& operator=(const CrostiniLowDiskNotification&) =
      delete;

  // Unregisters from observing events.
  ~CrostiniLowDiskNotification() override;

  // ash::CiceroneClient::Observer override.
  void OnLowDiskSpaceTriggered(
      const vm_tools::cicerone::LowDiskSpaceTriggeredSignal& signal) override;

  // This is responsible for deciding whether a notification should be shown or
  // not and showing it if appropriate. This must be called from the thread that
  // instantiated this object.
  void ShowNotificationIfAppropriate(uint64_t free_bytes);

 private:
  friend class CrostiniLowDiskNotificationTest;

  enum class Severity { NONE = -1, MEDIUM = 0, HIGH = 1 };

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
  Severity last_notification_severity_ = Severity::NONE;
  base::TimeDelta notification_interval_;
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<CrostiniLowDiskNotification> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_LOW_DISK_NOTIFICATION_H_
