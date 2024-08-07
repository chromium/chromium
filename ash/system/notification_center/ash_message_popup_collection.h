// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_POPUP_COLLECTION_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_POPUP_COLLECTION_H_

#include <stdint.h>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/tray/system_tray_observer.h"
#include "ash/system/tray/tray_event_filter.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/widget/widget_observer.h"

namespace display {
class Screen;
enum class TabletState;
}  // namespace display

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AshMessagePopupCollectionTest;
class Shelf;
class TrayBubbleView;
class TrayEventFilterTest;

// The MessagePopupCollection subclass for Ash. It needs to handle alignment of
// the shelf and its autohide state.
class ASH_EXPORT AshMessagePopupCollection
    : public display::DisplayObserver,
      public message_center::MessagePopupCollection,
      public message_center::MessageView::Observer,
      public views::WidgetObserver {
 public:
  // The name that will set for the message popup widget in
  // ConfigureWidgetInitParamsForContainer(), and that can be used to identify a
  // message popup widget.
  static const char kMessagePopupWidgetName[];

  // All the types of surfaces that can make popup collection shift up. Used
  // inside of `NotifierCollisionHandler` for metrics collection. Make sure to
  // keep this in sync with `NotifierCollisionSurfaceType` in
  // tools/metrics/histograms/metadata/ash/enums.xml.
  enum class NotifierCollisionSurfaceType {
    // Default value. Ideally this should never be recorded in the metrics.
    kNone = 0,

    kShelfPodBubble = 1,
    kSliderBubble = 2,
    kExtendedHotseat = 3,
    kSliderBubbleAndExtendedHotseat = 4,
    kMaxValue = kSliderBubbleAndExtendedHotseat
  };

  AshMessagePopupCollection(display::Screen* screen, Shelf* shelf);

  AshMessagePopupCollection(const AshMessagePopupCollection&) = delete;
  AshMessagePopupCollection& operator=(const AshMessagePopupCollection&) =
      delete;

  ~AshMessagePopupCollection() override;

  // Start observing the system.
  void StartObserving(display::Screen* screen, const display::Display& display);

  // message_center::MessagePopupCollection:
  int GetPopupOriginX(const gfx::Rect& popup_bounds) const override;
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
  void NotifySilentNotification(const std::string& notification_id) override;
  void NotifyPopupCollectionHeightChanged() override;
  void AnimationStarted() override;
  void AnimationFinished() override;
  message_center::MessagePopupView* CreatePopup(
      const message_center::Notification& notification) override;
  void ClosePopupItem(PopupItem& item) override;

  // Returns true if `widget` is a popup widget belongs to this popup
  // collection.
  bool IsWidgetAPopupNotification(views::Widget* widget);

  // Sets `animation_idle_closure_`.
  void SetAnimationIdleClosureForTest(base::OnceClosure closure);

  int popups_animating_for_test() const { return popups_animating_; }

 private:
  friend class AshMessagePopupCollectionTest;
  friend class NotificationGroupingControllerTest;
  friend class TrayEventFilterTest;

  // Handles the collision of popup notifications with corner anchored shelf pod
  // bubbles, sliders, shelf, and the extended hotseat by updating the popup
  // baseline.
  class NotifierCollisionHandler : public ShelfObserver,
                                   public SystemTrayObserver,
                                   public display::DisplayObserver {
   public:
    explicit NotifierCollisionHandler(
        AshMessagePopupCollection* popup_collection);

    NotifierCollisionHandler(const NotifierCollisionHandler&) = delete;
    NotifierCollisionHandler& operator=(const NotifierCollisionHandler&) =
        delete;

    ~NotifierCollisionHandler() override;

    // Triggered whenever the height of the popup collection changes.
    void OnPopupCollectionHeightChanged();

    // Calculates the offset that is applied to the popup collection's baseline.
    // It considers the extended hotseat, corner anchored shelf pod bubbles and
    // slider bubbles.
    int CalculateBaselineOffset();

    // SystemTrayObserver:
    void OnFocusLeavingSystemTray(bool reverse) override {}
    void OnStatusAreaAnchoredBubbleVisibilityChanged(
        TrayBubbleView* tray_bubble,
        bool visible) override;
    void OnTrayBubbleBoundsChanged(TrayBubbleView* tray_bubble) override;

   private:
    // Handles bubble visibility or bounds changes.
    void HandleBubbleVisibilityOrBoundsChanged();

    // Calculates the baseline offset applied when the hotseat is extended in
    // tablet mode and a corner anchored shelf pod bubble is not open.
    int CalculateExtendedHotseatOffset() const;

    // Calculates the baseline offset applied when a slider is visible and a
    // corner anchored shelf pod bubble is not open.
    int CalculateSliderOffset() const;

    // Records metrics for the count of popups when it is put on top of a
    // surface.
    void RecordOnTopOfSurfacesPopupCount();

    // Records surface type when there are popup(s) on top of that surface.
    void RecordSurfaceType();

    // display::DisplayObserver:
    void OnDisplayTabletStateChanged(display::TabletState state) override;

    // ShelfObserver:
    void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                                 AnimationChangeType change_type) override;
    void OnShelfWorkAreaInsetsChanged() override;
    void OnHotseatStateChanged(HotseatState old_state,
                               HotseatState new_state) override;

    raw_ptr<AshMessagePopupCollection> const popup_collection_;

    // Keeps track of the current baseline offset and surface type for metrics
    // collection purpose.
    int baseline_offset_ = 0;
    NotifierCollisionSurfaceType surface_type_ =
        NotifierCollisionSurfaceType::kNone;

    // True if bubble changes are being handled in
    // `HandleBubbleVisibilityOrBoundsChanged()`.
    bool is_handling_bubble_change_ = false;

    display::ScopedDisplayObserver display_observer_{this};
  };

  // message_center::MessageView::Observer:
  void OnSlideOut(const std::string& notification_id) override;
  void OnCloseButtonPressed(const std::string& notification_id) override;
  void OnSettingsButtonPressed(const std::string& notification_id) override;
  void OnSnoozeButtonPressed(const std::string& notification_id) override;

  // Gets the current alignment of the shelf.
  ShelfAlignment GetAlignment() const;

  // Utility function to get the display which should be care about.
  display::Display GetCurrentDisplay() const;

  // Computes the new work area.
  void UpdateWorkArea();

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  std::unique_ptr<NotifierCollisionHandler> notifier_collision_handler_;

  std::optional<display::ScopedDisplayObserver> display_observer_;

  raw_ptr<display::Screen> screen_;
  gfx::Rect work_area_;

  // Outlives this class.
  raw_ptr<Shelf> shelf_;

  std::set<raw_ptr<views::Widget, SetExperimental>> tracked_widgets_;

  // Tracks the smoothness of popup animation.
  std::optional<ui::ThroughputTracker> animation_tracker_;

  // Keeps track of number of items that are animating. This is used when we
  // have more than one popup appear in the screen and different animations are
  // performed at the same time (fade in, move up, etc.), making sure that we
  // stop the throughput tracker only when all of these animations are finished.
  int popups_animating_ = 0;

  // A closure called when all item animations complete. Used for tests only.
  base::OnceClosure animation_idle_closure_;

  // Keeps track the last pop up added, used by throughout tracker. We only
  // record smoothness when this variable is in scope.
  raw_ptr<message_center::MessagePopupView> last_pop_up_added_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_POPUP_COLLECTION_H_
