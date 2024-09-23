// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"

namespace message_center {
class MessagePopupView;
}  // namespace message_center

namespace views {
class ScrollView;
class View;
}  // namespace views

namespace ash {

class AccessibilityDetailedView;

// Public test API for the system tray. Methods only apply to the system tray
// on the primary display.
class ASH_EXPORT SystemTrayTestApi {
 public:
  static std::unique_ptr<SystemTrayTestApi> Create();

  SystemTrayTestApi();
  ~SystemTrayTestApi();

  // Returns true if the system tray bubble menu is open.
  bool IsTrayBubbleOpen();

  // Returns true if the system tray bubble menu is expanded.
  bool IsTrayBubbleExpanded();

  // Shows the system tray bubble menu.
  void ShowBubble();

  // Closes the system tray bubble menu.
  void CloseBubble();

  // Collapse the system tray bubble menu.
  void CollapseBubble();

  // Expand the system tray bubble menu.
  void ExpandBubble();

  // Shows the submenu view for the given section of the bubble menu.
  void ShowAccessibilityDetailedView();
  void ShowNetworkDetailedView();

  // Returns the current `ash::AccessibilityDetailedView`. This assumes that the
  // accessibility detailed view is currently showing.
  AccessibilityDetailedView* GetAccessibilityDetailedView();

  // Returns true if the sub-view `view_id` exists in the bubble and is visible.
  // If |open_tray| is true, it also opens system tray bubble.
  bool IsBubbleViewVisible(int view_id, bool open_tray);

  // Returns true if the `TrayToggleButton` with ID `view_id` is toggled on,
  // false otherwise.
  bool IsToggleOn(int view_id);

  // Searches for a `views::View` having ID `view_id` in `GetMainBubbleView()`
  // and then scrolls it onto the screen to make it visible (if it is already
  // visible then no scrolling is performed). The view should be a descendant
  // of `scroll_view` (this is `DCHECK`ed).
  void ScrollToShowView(views::ScrollView* scroll_view, int view_id);

  // Clicks the sub-view |view_id| of `GetMainBubbleView()`.
  void ClickBubbleView(int view_id);

  // Returns the main bubble view.
  views::View* GetMainBubbleView();

  // Returns the tooltip for the sub-view `view_id` of `GetMainBubbleView()`, or
  // the empty string if the view does not exist.
  std::u16string GetBubbleViewTooltip(int view_id);

  // Returns the tooltip for the "Shut down" button, or the empty string if the
  // view does not exist.
  std::u16string GetShutdownButtonTooltip();

  // Returns the text for a sub-view `view_id` of `GetMainBubbleView()`, or the
  // empty string if the view does not exist. This method only works if the
  // bubble view is a label.
  std::u16string GetBubbleViewText(int view_id);

  // Get the notification pop up view based on the notification id.
  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

  // Returns true if the clock is using 24 hour time.
  bool Is24HourClock();

  // Taps on the Select-to-Speak tray.
  void TapSelectToSpeakTray();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_
