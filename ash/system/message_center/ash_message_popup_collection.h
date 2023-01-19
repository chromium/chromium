// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_MESSAGE_POPUP_COLLECTION_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_MESSAGE_POPUP_COLLECTION_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "base/functional/callback_forward.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/widget/widget_observer.h"

namespace display {
class Screen;
}

namespace ash {

class AshMessagePopupCollectionTest;
class Shelf;

// The MessagePopupCollection subclass for Ash. It needs to handle alignment of
// the shelf and its autohide state.
class ASH_EXPORT AshMessagePopupCollection
    : public message_center::MessagePopupCollection,
      public ShelfObserver,
      public TabletModeObserver,
      public display::DisplayObserver,
      public views::WidgetObserver,
      public message_center::MessageView::Observer {
 public:
  // The name that will set for the message popup widget in
  // ConfigureWidgetInitParamsForContainer(), and that can be used to identify a
  // message popup widget.
  static const char kMessagePopupWidgetName[];

  explicit AshMessagePopupCollection(Shelf* shelf);

  AshMessagePopupCollection(const AshMessagePopupCollection&) = delete;
  AshMessagePopupCollection& operator=(const AshMessagePopupCollection&) =
      delete;

  ~AshMessagePopupCollection() override;

  // Start observing the system.
  void StartObserving(display::Screen* screen, const display::Display& display);

  // Sets the current height of the system tray bubble (or legacy notification
  // bubble) so that notification toasts can avoid it.
  void SetTrayBubbleHeight(int height);

  // message_center::MessagePopupCollection:
  int GetToastOriginX(const gfx::Rect& toast_bounds) const override;
  int GetBaseline() const override;
  gfx::Rect GetWorkArea() const override;
  bool IsTopDown() const override;
  bool IsFromLeft() const override;
  bool RecomputeAlignment(const display::Display& display) override;
  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      views::Widget::InitParams* init_params) override;
  bool IsPrimaryDisplayForNotification() const override;
  bool BlockForMixedFullscreen(
      const message_center::Notification& notification) const override;
  void NotifyPopupAdded(message_center::MessagePopupView* popup) override;
  void NotifyPopupClosed(message_center::MessagePopupView* popup) override;
  void AnimationStarted() override;
  void AnimationFinished() override;
  message_center::MessagePopupView* CreatePopup(
      const message_center::Notification& notification) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Sets `animation_idle_closure_`.
  void SetAnimationIdleClosureForTest(base::OnceClosure closure);

  // Returns the current tray bubble height or 0 if there is no bubble.
  int tray_bubble_height_for_test() const { return tray_bubble_height_; }

  int popups_animating_for_test() const { return popups_animating_; }

 private:
  friend class AshMessagePopupCollectionTest;
  friend class NotificationGroupingControllerTest;

  // message_center::MessageView::Observer:
  void OnSlideOut(const std::string& notification_id) override;
  void OnCloseButtonPressed(const std::string& notification_id) override;
  void OnSettingsButtonPressed(const std::string& notification_id) override;
  void OnSnoozeButtonPressed(const std::string& notification_id) override;

  // Get the current alignment of the shelf.
  ShelfAlignment GetAlignment() const;

  // Utility function to get the display which should be care about.
  display::Display GetCurrentDisplay() const;

  // Compute the new work area.
  void UpdateWorkArea();

  // ShelfObserver:
  void OnShelfWorkAreaInsetsChanged() override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  absl::optional<display::ScopedDisplayObserver> display_observer_;

  display::Screen* screen_;
  gfx::Rect work_area_;
  Shelf* shelf_;
  int tray_bubble_height_;

  std::set<views::Widget*> tracked_widgets_;

  // Tracks the smoothness of popup animation.
  absl::optional<ui::ThroughputTracker> animation_tracker_;

  // Keeps track of number of items that are animating. This is used when we
  // have more than one popup appear in the screen and different animations are
  // performed at the same time (fade in, move up, etc.), making sure that we
  // stop the throughput tracker only when all of these animations are finished.
  int popups_animating_ = 0;

  // A closure called when all item animations complete. Used for tests only.
  base::OnceClosure animation_idle_closure_;

  // Keeps track the last pop up added, used by throughout tracker. We only
  // record smoothness when this variable is in scope.
  message_center::MessagePopupView* last_pop_up_added_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_MESSAGE_POPUP_COLLECTION_H_
