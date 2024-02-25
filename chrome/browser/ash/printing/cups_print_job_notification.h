// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace base {
class OneShotTimer;
}

namespace message_center {
class Notification;
}

namespace ash {

class CupsPrintJob;
class CupsPrintJobNotificationManager;

// CupsPrintJobNotification is used to update the notification of a print job
// according to its state and respond to the user's action.
class CupsPrintJobNotification : public message_center::NotificationObserver {
 public:
  CupsPrintJobNotification(CupsPrintJobNotificationManager* manager,
                           base::WeakPtr<CupsPrintJob> print_job,
                           Profile* profile);

  CupsPrintJobNotification(const CupsPrintJobNotification&) = delete;
  CupsPrintJobNotification& operator=(const CupsPrintJobNotification&) = delete;

  virtual ~CupsPrintJobNotification();

  void OnPrintJobStatusUpdated();

  // message_center::NotificationObserver
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  message_center::Notification* GetNotificationDataForTesting();

 private:
  // Update the notification based on the print job's status.
  void UpdateNotification();
  void UpdateNotificationTitle();
  void UpdateNotificationIcon();
  void UpdateNotificationBodyMessage();

  void CleanUpNotification();

  raw_ptr<CupsPrintJobNotificationManager> notification_manager_;
  std::unique_ptr<message_center::Notification> notification_;
  std::string notification_id_;
  base::WeakPtr<CupsPrintJob> print_job_;
  raw_ptr<Profile> profile_;

  // Whether this print job has been submitted via the Web Printing API.
  // This field is separate from `print_job_` since the WeakPtr might expire
  // while the notification is still being shown, which could affect the logic
  // of handling clicks.
  const bool is_web_printing_api_initiated_;

  // If the notification has been closed in the middle of printing or not. If it
  // is true, then prevent the following print job progress update after close,
  // and only show the print job done or failed notification.
  bool closed_in_middle_ = false;

  // Timer to close the notification in case of success.
  std::unique_ptr<base::OneShotTimer> success_timer_;

  base::WeakPtrFactory<CupsPrintJobNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_H_
