// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_DATE_TRAY_H_
#define ASH_SYSTEM_UNIFIED_DATE_TRAY_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

class Shelf;
class TimeTrayItemView;
class TrayBubbleView;
class GlanceableTrayBubble;

// This date tray is next to the `UnifiedSystemTray`. Activating this tray
// results in the `CalendarView` showing in the `UnifiedSystemTray`'s bubble.
// If GlanceablesV2 feature flag is enabled, it will instead show the
// GlanceableTrayBubble.
// TODO(b:277268122) update documentation.
class ASH_EXPORT DateTray : public TrayBackgroundView,
                            public UnifiedSystemTray::Observer {
 public:
  METADATA_HEADER(DateTray);

  DateTray(Shelf* shelf, UnifiedSystemTray* tray);
  DateTray(const DateTray&) = delete;
  DateTray& operator=(const DateTray&) = delete;
  ~DateTray() override;

  // TrayBackgroundView:
  std::u16string GetAccessibleNameForBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void UpdateLayout() override;
  void UpdateAfterLoginStatusChange() override;
  void ShowBubble() override;
  void CloseBubble() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble() override;
  void UpdateTrayItemColor(bool is_active) override;

  // UnifiedSystemTray::Observer:
  void OnOpeningCalendarView() override;
  void OnLeavingCalendarView() override;

  // Callback called when this tray is pressed.
  void OnButtonPressed(const ui::Event& event);

  void ShowGlanceableBubble();
  void HideGlanceableBubble();

 private:
  friend class DateTrayTest;
  friend class GlanceablesPixelTest;

  // Owned by the views hierarchy.
  raw_ptr<TimeTrayItemView, ExperimentalAsh> time_view_ = nullptr;

  // Owned by `StatusAreaWidget`.
  raw_ptr<UnifiedSystemTray, ExperimentalAsh> unified_system_tray_ = nullptr;

  // Bubble container for Glanceable UI.
  std::unique_ptr<GlanceableTrayBubble> bubble_;

  base::ScopedObservation<UnifiedSystemTray, UnifiedSystemTray::Observer>
      scoped_unified_system_tray_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_DATE_TRAY_H_
