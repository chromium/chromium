// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_HEADER_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_HEADER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {

// The header view which shows the "Notifications" text and a counter to show
// the number of notifications for this app.
class NotificationMenuHeaderView : public views::View {
  METADATA_HEADER(NotificationMenuHeaderView, views::View)

 public:
  NotificationMenuHeaderView();

  NotificationMenuHeaderView(const NotificationMenuHeaderView&) = delete;
  NotificationMenuHeaderView& operator=(const NotificationMenuHeaderView&) =
      delete;

  ~NotificationMenuHeaderView() override;

  void UpdateCounter(int number_of_notifications);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

 private:
  friend class NotificationMenuViewTestAPI;

  // The number of notifications that are active for this application.
  int number_of_notifications_ = 0;

  // Holds the "Notifications" label. Owned by the views hierarchy.
  raw_ptr<views::Label> notification_title_ = nullptr;

  // Holds a numeric string that indicates how many notifications are active.
  // Owned by the views hierarchy.
  raw_ptr<views::Label> counter_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_HEADER_VIEW_H_
