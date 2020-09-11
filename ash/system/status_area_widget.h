// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_component.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ash {
class BloomTray;
class DictationButtonTray;
class HoldingSpaceTray;
class ImeMenuTray;
class LogoutButtonTray;
class MediaTray;
class OverviewButtonTray;
class PaletteTray;
class PhoneHubTray;
class SelectToSpeakTray;
class Shelf;
class StatusAreaOverflowButtonTray;
class StatusAreaWidgetDelegate;
class StopRecordingButtonTray;
class TrayBackgroundView;
class UnifiedSystemTray;
class VirtualKeyboardTray;

// Widget showing the system tray, notification tray, and other tray views in
// the bottom-right of the screen. Exists separately from ShelfView/ShelfWidget
// so that it can be shown in cases where the rest of the shelf is hidden (e.g.
// on secondary monitors at the login screen).
class ASH_EXPORT StatusAreaWidget : public SessionObserver,
                                    public ShelfComponent,
                                    public views::Widget {
 public:
  // Whether the status area is collapsed or expanded. Currently, this is only
  // applicable in in-app tablet mode. Otherwise the state is NOT_COLLAPSIBLE.
  enum class CollapseState { NOT_COLLAPSIBLE, COLLAPSED, EXPANDED };

  class ScopedTrayBubbleCounter {
   public:
    explicit ScopedTrayBubbleCounter(StatusAreaWidget* status_area_widget);
    ~ScopedTrayBubbleCounter();

   private:
    base::WeakPtr<StatusAreaWidget> status_area_widget_;
  };

  StatusAreaWidget(aura::Window* status_container, Shelf* shelf);
  ~StatusAreaWidget() override;

  // Returns the status area widget for the display that |window| is on.
  static StatusAreaWidget* ForWindow(aura::Window* window);

  // Creates the child tray views, initializes them, and shows the widget. Not
  // part of the constructor because some child views call back into this object
  // during construction.
  void Initialize();

  // Called by the client when the login status changes. Caches login_status
  // and calls UpdateAfterLoginStatusChange for the system tray and the web
  // notification tray.
  void UpdateAfterLoginStatusChange(LoginStatus login_status);

  // Updates the collapse state of the status area after the state of the shelf
  // changes.
  void UpdateCollapseState();

  // Logs the number of visible status area item pods. Called after the a pod
  // changes visibility.
  void LogVisiblePodCountMetric();

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // Called by shelf layout manager when a locale change has been detected.
  void HandleLocaleChange();

  // Sets system tray visibility. Shows or hides widget if needed.
  void SetSystemTrayVisibility(bool visible);

  // Get the tray button that the system tray bubble and the notification center
  // bubble will be anchored. Usually |unified_system_tray_|, but when the
  // overview button is visible (i.e. tablet mode is enabled), it returns
  // |overview_button_tray_|.
  TrayBackgroundView* GetSystemTrayAnchor() const;

  StatusAreaWidgetDelegate* status_area_widget_delegate() {
    return status_area_widget_delegate_;
  }
  UnifiedSystemTray* unified_system_tray() {
    return unified_system_tray_.get();
  }
  DictationButtonTray* dictation_button_tray() {
    return dictation_button_tray_.get();
  }
  MediaTray* media_tray() { return media_tray_.get(); }
  StatusAreaOverflowButtonTray* overflow_button_tray() {
    return overflow_button_tray_.get();
  }
  OverviewButtonTray* overview_button_tray() {
    return overview_button_tray_.get();
  }
  PaletteTray* palette_tray() { return palette_tray_.get(); }
  StopRecordingButtonTray* stop_recording_button_tray() {
    return stop_recording_button_tray_.get();
  }
  ImeMenuTray* ime_menu_tray() { return ime_menu_tray_.get(); }
  HoldingSpaceTray* holding_space_tray() { return holding_space_tray_.get(); }
  PhoneHubTray* phone_hub_tray() { return phone_hub_tray_.get(); }

  SelectToSpeakTray* select_to_speak_tray() {
    return select_to_speak_tray_.get();
  }

  Shelf* shelf() { return shelf_; }

  LoginStatus login_status() const { return login_status_; }

  // Returns true if the shelf should be visible. This is used when the
  // shelf is configured to auto-hide and test if the shelf should force
  // the shelf to remain visible.
  bool ShouldShowShelf() const;

  // True if any message bubble is shown.
  bool IsMessageBubbleShown() const;

  // Notifies child trays, and the |status_area_widget_delegate_| to schedule a
  // paint.
  void SchedulePaint();

  // Overridden from views::Widget:
  const ui::NativeTheme* GetNativeTheme() const override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // TODO(jamescook): Introduce a test API instead of these methods.
  LogoutButtonTray* logout_button_tray_for_testing() {
    return logout_button_tray_.get();
  }
  VirtualKeyboardTray* virtual_keyboard_tray_for_testing() {
    return virtual_keyboard_tray_.get();
  }

  BloomTray* bloom_tray_for_testing() { return bloom_tray_.get(); }

  CollapseState collapse_state() const { return collapse_state_; }
  void set_collapse_state_for_test(CollapseState state) {
    collapse_state_ = state;
  }

 private:
  struct LayoutInputs {
    gfx::Rect bounds;
    CollapseState collapse_state = CollapseState::NOT_COLLAPSIBLE;
    float opacity = 0.0f;
    // Each bit keep track of one child's visibility.
    unsigned int child_visibility_bitmask = 0;

    // Indicates whether animation is allowed.
    bool should_animate = true;

    // |should_animate| does not affect the status area widget's target
    // layout. So it is not taken into consideration when comparing LayoutInputs
    // instances.
    bool operator==(const LayoutInputs& other) const {
      return bounds == other.bounds && collapse_state == other.collapse_state &&
             opacity == other.opacity &&
             child_visibility_bitmask == other.child_visibility_bitmask;
    }
  };

  // Collects the inputs for layout.
  LayoutInputs GetLayoutInputs() const;

  // The set of inputs that impact this widget's layout. The assumption is that
  // this widget needs a relayout if, and only if, one or more of these has
  // changed.
  base::Optional<LayoutInputs> layout_inputs_;

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // Adds a new tray button to the status area.
  void AddTrayButton(TrayBackgroundView* tray_button);

  // Update the colors used for the tray buttons.
  void UpdateAfterColorModeChange();

  // Called when in the collapsed state to calculate and update the visibility
  // of each tray button.
  void CalculateButtonVisibilityForCollapsedState();

  // Calculates and returns the appropriate collapse state depending on
  // current conditions.
  CollapseState CalculateCollapseState() const;

  StatusAreaWidgetDelegate* status_area_widget_delegate_;

  std::unique_ptr<StatusAreaOverflowButtonTray> overflow_button_tray_;
  std::unique_ptr<OverviewButtonTray> overview_button_tray_;
  std::unique_ptr<DictationButtonTray> dictation_button_tray_;
  std::unique_ptr<MediaTray> media_tray_;
  std::unique_ptr<UnifiedSystemTray> unified_system_tray_;
  std::unique_ptr<LogoutButtonTray> logout_button_tray_;
  std::unique_ptr<PaletteTray> palette_tray_;
  std::unique_ptr<PhoneHubTray> phone_hub_tray_;
  std::unique_ptr<StopRecordingButtonTray> stop_recording_button_tray_;
  std::unique_ptr<VirtualKeyboardTray> virtual_keyboard_tray_;
  std::unique_ptr<BloomTray> bloom_tray_;
  std::unique_ptr<ImeMenuTray> ime_menu_tray_;
  std::unique_ptr<SelectToSpeakTray> select_to_speak_tray_;
  std::unique_ptr<HoldingSpaceTray> holding_space_tray_;

  // Vector of the tray buttons above. The ordering is used to determine which
  // tray buttons are hidden when they overflow the available width.
  std::vector<TrayBackgroundView*> tray_buttons_;

  LoginStatus login_status_ = LoginStatus::NOT_LOGGED_IN;

  CollapseState collapse_state_ = CollapseState::NOT_COLLAPSIBLE;

  gfx::Rect target_bounds_;

  Shelf* shelf_;

  bool initialized_ = false;

  // Number of active tray bubbles on the display where status area widget
  // lives.
  int tray_bubble_count_ = 0;

  base::WeakPtrFactory<StatusAreaWidget> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
