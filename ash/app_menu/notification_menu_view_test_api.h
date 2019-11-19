// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_VIEW_TEST_API_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_VIEW_TEST_API_H_

#include "base/macros.h"
#include "base/strings/string16.h"

namespace ash {

class NotificationMenuView;
class NotificationOverflowView;

// Use the API in this class to test NotificationMenuView.
class NotificationMenuViewTestAPI {
 public:
  explicit NotificationMenuViewTestAPI(
      NotificationMenuView* notification_menu_view);
  ~NotificationMenuViewTestAPI();

  // Returns the numeric string contained in the counter view.
  base::string16 GetCounterViewContents() const;

  // Returns the number of NotificationItemViews.
  int GetItemViewCount() const;

  // Returns the NotificationOverflowView if it is being shown.
  NotificationOverflowView* GetOverflowView() const;

 private:
  NotificationMenuView* const notification_menu_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMenuViewTestAPI);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_VIEW_TEST_API_H_
