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
// results in the `GlanceableTrayBubble` showing in the `UnifiedSystemTray`'s
// bubble.
class ASH_EXPORT DateTray : public TrayBackgroundView,
                            public UnifiedSystemTray::Observer {
  METADATA_HEADER(DateTray, TrayBackgroundView)

 public:
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
  void CloseBubbleInternal() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;

  // UnifiedSystemTray::Observer:
  void OnOpeningCalendarView() override;
  void OnLeavingCalendarView() override;

  // Callback called when this tray is pressed.
  void OnButtonPressed(const ui::Event& event);

  // `from_keyboard` - whether `ShowGlanceableBubble()` is being shown in
  // response to a keyboard event.
  void ShowGlanceableBubble(bool from_keyboard);
  void HideGlanceableBubble();

  GlanceableTrayBubble* glanceables_bubble_for_test() const {
    return bubble_.get();
  }

 private:
  friend class DateTrayTest;

  // Owned by the views hierarchy.
  raw_ptr<TimeTrayItemView> time_view_ = nullptr;

  // Owned by `StatusAreaWidget`.
  raw_ptr<UnifiedSystemTray> unified_system_tray_ = nullptr;

  // Bubble container for Glanceable UI.
  std::unique_ptr<GlanceableTrayBubble> bubble_;

  base::ScopedObservation<UnifiedSystemTray, UnifiedSystemTray::Observer>
      scoped_unified_system_tray_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_DATE_TRAY_H_
