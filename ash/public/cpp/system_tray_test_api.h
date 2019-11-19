// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace ash {

// Public test API for the system tray. Methods only apply to the system tray
// on the primary display.
class ASH_EXPORT SystemTrayTestApi {
 public:
  static std::unique_ptr<SystemTrayTestApi> Create();

  virtual ~SystemTrayTestApi() {}

  // Disables animations (e.g. the tray view icon slide-in).
  virtual void DisableAnimations() = 0;

  // Returns true if the system tray bubble menu is open.
  virtual bool IsTrayBubbleOpen() = 0;

  // Shows the system tray bubble menu.
  virtual void ShowBubble() = 0;

  // Closes the system tray bubble menu.
  virtual void CloseBubble() = 0;

  // Shows the submenu view for the given section of the bubble menu.
  virtual void ShowAccessibilityDetailedView() = 0;
  virtual void ShowNetworkDetailedView() = 0;

  // Returns true if the view exists in the bubble and is visible.
  // If |open_tray| is true, it also opens system tray bubble.
  virtual bool IsBubbleViewVisible(int view_id, bool open_tray) = 0;

  // Clicks the view |view_id|.
  virtual void ClickBubbleView(int view_id) = 0;

  // Returns the tooltip for a bubble view, or the empty string if the view
  // does not exist.
  virtual base::string16 GetBubbleViewTooltip(int view_id) = 0;

  // Returns true if the clock is using 24 hour time.
  virtual bool Is24HourClock() = 0;

 protected:
  SystemTrayTestApi() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_
