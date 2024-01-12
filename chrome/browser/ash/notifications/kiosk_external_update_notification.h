// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_

#include <string>

#include "base/memory/raw_ptr.h"

namespace ash {

class KioskExternalUpdateNotificationView;

// Provides the UI showing kiosk external update status to admin.
class KioskExternalUpdateNotification {
 public:
  explicit KioskExternalUpdateNotification(const std::u16string& message);

  KioskExternalUpdateNotification(const KioskExternalUpdateNotification&) =
      delete;
  KioskExternalUpdateNotification& operator=(
      const KioskExternalUpdateNotification&) = delete;

  virtual ~KioskExternalUpdateNotification();

  void ShowMessage(const std::u16string& message);

 private:
  friend class KioskExternalUpdateNotificationView;
  void Dismiss();
  void CreateAndShowNotificationView(const std::u16string& message);

  raw_ptr<KioskExternalUpdateNotificationView>
      view_;  // Owned by views hierarchy.
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_
