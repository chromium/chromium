// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {

// Maximum count of notification shown by a number label. "+" icon is shown
// instead if it exceeds this limit.
constexpr size_t kTrayNotificationMaxCount = 9;

// A notification counter view in UnifiedSystemTray button.
class ASH_EXPORT NotificationCounterView : public TrayItemView,
                                           public SessionObserver {
 public:
  explicit NotificationCounterView(Shelf* shelf);
  ~NotificationCounterView() override;

  void Update();

  // Returns a string describing the current state for accessibility.
  base::string16 GetAccessibleNameString() const;

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::TrayItemView:
  const char* GetClassName() const override;

  int count_for_display_for_testing() const { return count_for_display_; }

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

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::TrayItemView:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuietModeView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
