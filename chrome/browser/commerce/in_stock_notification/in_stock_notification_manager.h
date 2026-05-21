// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_IN_STOCK_NOTIFICATION_IN_STOCK_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_COMMERCE_IN_STOCK_NOTIFICATION_IN_STOCK_NOTIFICATION_MANAGER_H_

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace commerce {

class InStockNotificationManager {
 public:
  DECLARE_USER_DATA(InStockNotificationManager);

  explicit InStockNotificationManager(tabs::TabInterface* tab);
  InStockNotificationManager(const InStockNotificationManager&) = delete;
  void operator=(const InStockNotificationManager&) = delete;
  ~InStockNotificationManager();

 private:
  ui::ScopedUnownedUserData<InStockNotificationManager>
      scoped_unowned_user_data_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_IN_STOCK_NOTIFICATION_IN_STOCK_NOTIFICATION_MANAGER_H_
