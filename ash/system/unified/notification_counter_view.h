// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_item_view.h"

namespace session_manager {
enum class SessionState;
}  // namespace session_manager

namespace views {
class Separator;
}  // namespace views

namespace ash {

class NotificationIconsController;

// Maximum count of notification shown by a number label. "+" icon is shown
// instead if it exceeds this limit.
constexpr size_t kTrayNotificationMaxCount = 9;

// A notification counter view in UnifiedSystemTray button.
class ASH_EXPORT NotificationCounterView : public TrayItemView {
 public:
  NotificationCounterView(Shelf* shelf,
                          NotificationIconsController* controller);
  ~NotificationCounterView() override;
  NotificationCounterView(const NotificationCounterView&) = delete;
  NotificationCounterView& operator=(const NotificationCounterView&) = delete;

  void Update();

  // Returns a string describing the current state for accessibility.
  std::u16string GetAccessibleNameString() const;

  // TrayItemView:
  void HandleLocaleChange() override;
  void OnThemeChanged() override;

  // views::TrayItemView:
  const char* GetClassName() const override;

  int count_for_display_for_testing() const { return count_for_display_; }

 private:
  // The type / number of the icon that is currently set to the image view.
  // 0 indicates no icon is drawn yet.
  // 1 through |kTrayNotificationMaxCount| indicates each number icons.
  // |kTrayNotificationMaxCount| + 1 indicates the plus icon.
  int count_for_display_ = 0;

  NotificationIconsController* const controller_;
};

// A do-not-distrub icon view in UnifiedSystemTray button.
class QuietModeView : public TrayItemView {
 public:
  explicit QuietModeView(Shelf* shelf);
  ~QuietModeView() override;
  QuietModeView(const QuietModeView&) = delete;
  QuietModeView& operator=(const QuietModeView&) = delete;

  void Update();

  // TrayItemView:
  void HandleLocaleChange() override;
  void OnThemeChanged() override;

  // views::TrayItemView:
  const char* GetClassName() const override;
};

// Separator view in UnifiedSystemTray button.
class SeparatorTrayItemView : public TrayItemView {
 public:
  explicit SeparatorTrayItemView(Shelf* shelf);
  ~SeparatorTrayItemView() override;
  SeparatorTrayItemView(const SeparatorTrayItemView&) = delete;
  SeparatorTrayItemView& operator=(const SeparatorTrayItemView&) = delete;

  // TrayItemView:
  void HandleLocaleChange() override;
  const char* GetClassName() const override;

  // Update the color of separator depending on the given state.
  void UpdateColor(session_manager::SessionState state);

 private:
  views::Separator* separator_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_COUNTER_VIEW_H_
