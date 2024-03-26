// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_H_

#include "ash/system/tray/tray_bubble_base.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/unified/date_tray.h"
#include "base/memory/raw_ptr.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class CalendarView;
class GlanceableTrayBubbleView;

// Manages the bubble that contains GlanceableTrayView.
// Shows the bubble on the constructor, and closes the bubble on the destructor.
class ASH_EXPORT GlanceableTrayBubble : public TrayBubbleBase {
 public:
  // `from_keyboard` - whether the bubble is shown in response to a keyboard
  // event, in which case the bubble should be activated when shown.
  GlanceableTrayBubble(DateTray* tray, bool from_keyboard);

  GlanceableTrayBubble(const GlanceableTrayBubble&) = delete;
  GlanceableTrayBubble& operator=(const GlanceableTrayBubble&) = delete;

  ~GlanceableTrayBubble() override;

  // TrayBubbleBase:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;
  bool IsBubbleActive() const;

  views::View* GetTasksView();
  views::View* GetClassroomStudentView();
  CalendarView* GetCalendarView();

 private:
  // Wrapper around `GetBubbleView()` that returns the bubble view as
  // `GlanceableTrayBubbleView`.
  GlanceableTrayBubbleView* GetGlanceableTrayBubbleView();

  void UpdateBubble();

  // Owner of this class.
  raw_ptr<DateTray> tray_;

  // Bubble wrapper that manages the bubble widget when shown.
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_H_
