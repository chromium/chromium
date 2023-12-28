// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_VIEW_TEST_API_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_VIEW_TEST_API_H_

#include <string>

#include "base/memory/raw_ptr.h"

namespace ash {

class NotificationMenuView;
class NotificationOverflowView;

// Use the API in this class to test NotificationMenuView.
class NotificationMenuViewTestAPI {
 public:
  explicit NotificationMenuViewTestAPI(
      NotificationMenuView* notification_menu_view);

  NotificationMenuViewTestAPI(const NotificationMenuViewTestAPI&) = delete;
  NotificationMenuViewTestAPI& operator=(const NotificationMenuViewTestAPI&) =
      delete;

  ~NotificationMenuViewTestAPI();

  // Returns the numeric string contained in the counter view.
  std::u16string GetCounterViewContents() const;

  // Returns the number of NotificationItemViews.
  int GetItemViewCount() const;

  // Returns the NotificationOverflowView if it is being shown.
  NotificationOverflowView* GetOverflowView() const;

 private:
  const raw_ptr<NotificationMenuView, DanglingUntriaged>
      notification_menu_view_;
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_VIEW_TEST_API_H_
