// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_DATE_TRAY_H_
#define ASH_SYSTEM_UNIFIED_DATE_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class TimeTrayItemView;

// This date tray is next to the `UnifidedSystemTray`. Activating this tray
// results in the CalendarView showing in the UnifiedSystemTray's bubble. This
// tray doesn't not have its own bubble.
class ASH_EXPORT DateTray : public TrayBackgroundView,
                            public UnifiedSystemTray::Observer {
 public:
  METADATA_HEADER(DateTray);

  DateTray(Shelf* shelf, UnifiedSystemTray* tray);
  DateTray(const DateTray&) = delete;
  DateTray& operator=(const DateTray&) = delete;
  ~DateTray() override;

  // TrayBackgroundView:
  bool PerformAction(const ui::Event& event) override;
  std::u16string GetAccessibleNameForBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void UpdateLayout() override;
  void UpdateAfterLoginStatusChange() override;
  void ShowBubble() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble() override {}

  // UnifiedSystemTray::Observer:
  void OnOpeningCalendarView() override;
  void OnLeavingCalendarView() override;

 private:
  friend class DateTrayTest;

  // Owned.
  TimeTrayItemView* time_view_ = nullptr;

  // Owned by `StatusAreaWidget`.
  UnifiedSystemTray* unified_system_tray_ = nullptr;

  base::ScopedObservation<UnifiedSystemTray, UnifiedSystemTray::Observer>
      scoped_unified_system_tray_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_DATE_TRAY_H_
