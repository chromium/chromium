// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_UTILS_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_UTILS_H_

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class CupsPrintJob;

namespace printing::internal {

// Updates the title of the CUPS notification based on the current state
// of `job`.
void UpdateNotificationTitle(message_center::Notification* notification,
                             const CupsPrintJob& job);

// Updates the icon of the CUPS notification based on the current state
// of `job`.
void UpdateNotificationIcon(message_center::Notification* notification,
                            const CupsPrintJob& job);

// Updates the body message of the CUPS notification based on the current state
// of `job`.
void UpdateNotificationBodyMessage(message_center::Notification* notification,
                                   const CupsPrintJob& job,
                                   Profile& profile);

}  // namespace printing::internal
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_UTILS_H_
