// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_

#include <string>

#include "base/macros.h"

namespace ash {

class KioskExternalUpdateNotificationView;

// Provides the UI showing kiosk external update status to admin.
class KioskExternalUpdateNotification {
 public:
  explicit KioskExternalUpdateNotification(const std::u16string& message);
  virtual ~KioskExternalUpdateNotification();

  void ShowMessage(const std::u16string& message);

 private:
  friend class KioskExternalUpdateNotificationView;
  void Dismiss();
  void CreateAndShowNotificationView(const std::u16string& message);

  KioskExternalUpdateNotificationView* view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(KioskExternalUpdateNotification);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_
