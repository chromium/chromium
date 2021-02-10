// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/macros.h"
#include "base/scoped_observation.h"

namespace ash {

class NotificationIconsController;
class UnifiedSystemTray;

// Maximum count of notification shown by a number label. "+" icon is shown
// instead if it exceeds this limit.
constexpr size_t kTrayNotificationMaxCount = 9;

// A notification counter view in UnifiedSystemTray button. This will be shown
// when there's notification and the tray doesn't show any notification icons.
class ASH_EXPORT NotificationCounterView
    : public TrayItemView,
      public SessionObserver,
      public UnifiedSystemTrayModel::Observer {
 public:
  NotificationCounterView(UnifiedSystemTray* tray,
                          NotificationIconsController* controller);
  ~NotificationCounterView() override;
  NotificationCounterView(const NotificationCounterView&) = delete;
  NotificationCounterView& operator=(const NotificationCounterView&) = delete;

  void Update();

  // Returns a string describing the current state for accessibility.
  base::string16 GetAccessibleNameString() const;

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // UnifiedSystemTrayModel::Observer:
  void OnSystemTrayButtonSizeChanged(
      UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) override;

  // views::TrayItemView:
  const char* GetClassName() const override;

  int count_for_display_for_testing() const { return count_for_display_; }

 private:
  // The type / number of the icon that is currently set to the image view.
  // 0 indicates no icon is drawn yet.
  // 1 through |kTrayNotificationMaxCount| indicates each number icons.
  // |kTrayNotificationMaxCount| + 1 indicates the plus icon.
  int count_for_display_ = 0;

  // Indicates if the notification icons view is set to be shown. Currently, we
  // show the icon view in medium or large screen size.
  bool icons_view_visible_ = false;

  NotificationIconsController* const controller_;

  base::ScopedObservation<UnifiedSystemTrayModel,
                          UnifiedSystemTrayModel::Observer>
      system_tray_model_observation_{this};
};

// An icon view to indicate the number of hidden notifications (besides from the
// notifications that are shown in tray).
class HiddenNotificationCountView : public TrayItemView {
 public:
  explicit HiddenNotificationCountView(Shelf* shelf);
  ~HiddenNotificationCountView() override;
  HiddenNotificationCountView(const HiddenNotificationCountView&) = delete;
  HiddenNotificationCountView& operator=(const HiddenNotificationCountView&) =
      delete;

  // TrayItemView:
  void HandleLocaleChange() override;
  const char* GetClassName() const override;
};

// A do-not-distrub icon view in UnifiedSystemTray button.
class QuietModeView : public TrayItemView, public SessionObserver {
 public:
  explicit QuietModeView(Shelf* shelf);
  ~QuietModeView() override;
  QuietModeView(const QuietModeView&) = delete;
  QuietModeView& operator=(const QuietModeView&) = delete;

  void Update();

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::TrayItemView:
  const char* GetClassName() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
