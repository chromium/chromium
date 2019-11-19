// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_

#include "ash/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {

// A notification counter view in UnifiedSystemTray button.
class NotificationCounterView : public TrayItemView, public SessionObserver {
 public:
  explicit NotificationCounterView(Shelf* shelf);
  ~NotificationCounterView() override;

  void Update();

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::TrayItemView:
  const char* GetClassName() const override;

 private:
  // The type / number of the icon that is currently set to the image view.
  // 0 indicates no icon is drawn yet.
  // 1 through |kTrayNotificationMaxCount| indicates each number icons.
  // |kTrayNotificationMaxCount| + 1 indicates the plus icon.
  int count_for_display_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NotificationCounterView);
};

// A do-not-distrub icon view in UnifiedSystemTray button.
class QuietModeView : public TrayItemView, public SessionObserver {
 public:
  explicit QuietModeView(Shelf* shelf);
  ~QuietModeView() override;

  void Update();

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::TrayItemView:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuietModeView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
