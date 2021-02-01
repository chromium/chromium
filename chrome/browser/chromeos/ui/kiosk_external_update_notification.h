// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_UI_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_

#include "base/macros.h"
#include "base/strings/string16.h"

namespace chromeos {

class KioskExternalUpdateNotificationView;

// Provides the UI showing kiosk external update status to admin.
class KioskExternalUpdateNotification {
 public:
  explicit KioskExternalUpdateNotification(const base::string16& message);
  virtual ~KioskExternalUpdateNotification();

  void ShowMessage(const base::string16& message);

 private:
  friend class KioskExternalUpdateNotificationView;
  void Dismiss();
  void CreateAndShowNotificationView(const base::string16& message);

  KioskExternalUpdateNotificationView* view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(KioskExternalUpdateNotification);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when migrated to
// chrome/browser/ash/.
namespace ash {
using ::chromeos::KioskExternalUpdateNotification;
}

#endif  // CHROME_BROWSER_CHROMEOS_UI_KIOSK_EXTERNAL_UPDATE_NOTIFICATION_H_
