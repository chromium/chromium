// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_TEST_API_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_TEST_API_H_

#include <memory>

#include "ash/public/cpp/system_tray_test_api.h"
#include "base/macros.h"

namespace ui {
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace message_center {
class MessagePopupView;
}  // namespace message_center

namespace ash {

class UnifiedSystemTray;

// Use by tests to access private state of UnifiedSystemTray.
// TODO(jamescook): Rename class to SystemTrayTestApiImpl.
class UnifiedSystemTrayTestApi : public SystemTrayTestApi {
 public:
  explicit UnifiedSystemTrayTestApi(UnifiedSystemTray* tray);
  ~UnifiedSystemTrayTestApi() override;

  // SystemTrayTestApi:
  void DisableAnimations() override;
  bool IsTrayBubbleOpen() override;
  void ShowBubble() override;
  void CloseBubble() override;
  void ShowAccessibilityDetailedView() override;
  void ShowNetworkDetailedView() override;
  bool IsBubbleViewVisible(int view_id, bool open_tray) override;
  void ClickBubbleView(int view_id) override;
  base::string16 GetBubbleViewTooltip(int view_id) override;
  bool Is24HourClock() override;

  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

 private:
  // Returns a view in the bubble menu (not the tray itself). Returns null if
  // not found.
  views::View* GetBubbleView(int view_id) const;

  UnifiedSystemTray* const tray_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animations_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_TEST_API_H_
