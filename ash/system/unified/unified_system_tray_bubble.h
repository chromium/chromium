// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_BUBBLE_H_

#include <memory>
#include <optional>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/tray/time_to_click_recorder.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "ash/system/unified/quick_settings_view.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

class TrayEventFilter;
class UnifiedSystemTray;
class UnifiedSystemTrayController;
class QuickSettingsView;

// Manages the bubble that contains 'QuickSettingsView'.
// Shows the bubble on the constructor, and closes the bubble on the destructor.
// It is possible that the bubble widget is closed on deactivation. In such
// case, this class calls UnifiedSystemTray::CloseBubble() to delete itself.
class ASH_EXPORT UnifiedSystemTrayBubble : public TrayBubbleBase,
                                           public ScreenLayoutObserver,
                                           public ShelfObserver,
                                           public TimeToClickRecorder::Delegate,
                                           public TabletModeObserver {
 public:
  explicit UnifiedSystemTrayBubble(UnifiedSystemTray* tray);

  UnifiedSystemTrayBubble(const UnifiedSystemTrayBubble&) = delete;
  UnifiedSystemTrayBubble& operator=(const UnifiedSystemTrayBubble&) = delete;

  ~UnifiedSystemTrayBubble() override;

  // Add observers that can delete `this`. This needs to be done separately
  // after `UnifiedSystemTrayBubble` and `UnifiedMessageCenterBubble` have been
  // completely constructed to prevent crashes. (crbug/1310675)
  void InitializeObservers();

  // Return the bounds of the bubble in the screen.
  gfx::Rect GetBoundsInScreen() const;

  // True if the bubble is active.
  bool IsBubbleActive() const;

  // Show audio settings detailed view.
  void ShowAudioDetailedView();

  // Show display settings detailed view.
  void ShowDisplayDetailedView();

  // Show calendar view.
  void ShowCalendarView(calendar_metrics::CalendarViewShowSource show_source,
                        calendar_metrics::CalendarEventSource event_source);

  // Show network settings detailed view.
  void ShowNetworkDetailedView();

  // Update bubble bounds and focus if necessary.
  void UpdateBubble();

  // Return the current visible height of the tray, even when partially
  // collapsed / expanded.
  int GetCurrentTrayHeight() const;

  // Fire a notification that an accessibility event has occured on this object.
  void NotifyAccessibilityEvent(ax::mojom::Event event, bool send_native_event);

  // Whether the bubble is currently showing audio details or display details or
  // calendar view.
  bool ShowingAudioDetailedView() const;
  bool ShowingDisplayDetailedView() const;
  bool ShowingCalendarView() const;

  // TrayBubbleBase:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // ScreenLayoutObserver:
  void OnDidApplyDisplayChanges() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // TimeToClickRecorder::Delegate:
  void RecordTimeToClick() override;

  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // Updates the bubble height based on if it's going to show the main page or
  // the detailed page.
  void UpdateBubbleHeight(bool is_showing_detiled_view);

  QuickSettingsView* quick_settings_view() { return quick_settings_view_; }

  UnifiedSystemTrayController* unified_system_tray_controller() {
    return controller_.get();
  }

 private:
  friend class SystemTrayTestApi;

  void UpdateBubbleBounds();

  // Controller of `QuickSettingsView`. As the view is owned by views hierarchy,
  // we have to own the controller here.
  std::unique_ptr<UnifiedSystemTrayController> controller_;

  // Owner of this class.
  raw_ptr<UnifiedSystemTray> unified_system_tray_;

  // Widget that contains `QuickSettingsView`. Unowned.
  // When the widget is closed by deactivation, `bubble_widget_` pointer is
  // invalidated and we have to delete `UnifiedSystemTrayBubble` by calling
  // `UnifiedSystemTray::CloseBubble()`.
  // In order to do this, we observe `OnWidgetDestroying()`.
  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  // PreTargetHandler of `quick_settings_view_` to record TimeToClick metrics.
  // Owned.
  std::unique_ptr<TimeToClickRecorder> time_to_click_recorder_;

  // The time the bubble is created.
  std::optional<base::TimeTicks> time_opened_;

  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;

  raw_ptr<QuickSettingsView, DanglingUntriaged> quick_settings_view_ = nullptr;

  std::unique_ptr<TrayEventFilter> tray_event_filter_;

  base::WeakPtrFactory<UnifiedSystemTrayBubble> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_BUBBLE_H_
