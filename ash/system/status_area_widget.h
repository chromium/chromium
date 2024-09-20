// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_component.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ash {

class DateTray;
class DictationButtonTray;
class EcheTray;
class FocusModeTray;
class HoldingSpaceTray;
class ImeMenuTray;
class LogoutButtonTray;
class MediaTray;
class NotificationCenterTray;
class OverviewButtonTray;
class PaletteTray;
class PhoneHubTray;
class PodsOverflowTray;
class AnnotationTray;
class MouseKeysTray;
class SelectToSpeakTray;
class Shelf;
class StatusAreaAnimationController;
class StatusAreaOverflowButtonTray;
class StatusAreaWidgetDelegate;
class StopRecordingButtonTray;
class TrayBackgroundView;
class TrayBubbleView;
class UnifiedSystemTray;
class VideoConferenceTray;
class VirtualKeyboardTray;
class WmModeButtonTray;

// Widget showing the system tray, notification tray, and other tray views in
// the bottom-right of the screen. Exists separately from ShelfView/ShelfWidget
// so that it can be shown in cases where the rest of the shelf is hidden (e.g.
// on secondary monitors at the login screen).
class ASH_EXPORT StatusAreaWidget : public SessionObserver,
                                    public ShelfComponent,
                                    public views::ViewObserver,
                                    public views::Widget {
 public:
  // Whether the status area is collapsed or expanded. Currently, this is only
  // applicable in in-app tablet mode. Otherwise the state is NOT_COLLAPSIBLE.
  enum class CollapseState { NOT_COLLAPSIBLE, COLLAPSED, EXPANDED };

  StatusAreaWidget(aura::Window* status_container, Shelf* shelf);

  StatusAreaWidget(const StatusAreaWidget&) = delete;
  StatusAreaWidget& operator=(const StatusAreaWidget&) = delete;

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
  PodsOverflowTray* pods_overflow_tray() { return pods_overflow_tray_; }
  UnifiedSystemTray* unified_system_tray() { return unified_system_tray_; }
  NotificationCenterTray* notification_center_tray() {
    return notification_center_tray_;
  }
  DateTray* date_tray() { return date_tray_; }
  DictationButtonTray* dictation_button_tray() {
    return dictation_button_tray_;
  }
  MediaTray* media_tray() { return media_tray_; }
  StatusAreaOverflowButtonTray* overflow_button_tray() {
    return overflow_button_tray_;
  }
  OverviewButtonTray* overview_button_tray() { return overview_button_tray_; }
  PaletteTray* palette_tray() { return palette_tray_; }
  VideoConferenceTray* video_conference_tray() {
    return video_conference_tray_;
  }
  StopRecordingButtonTray* stop_recording_button_tray() {
    return stop_recording_button_tray_;
  }
  FocusModeTray* focus_mode_tray() { return focus_mode_tray_; }
  AnnotationTray* annotation_tray() { return annotation_tray_; }
  ImeMenuTray* ime_menu_tray() { return ime_menu_tray_; }
  HoldingSpaceTray* holding_space_tray() { return holding_space_tray_; }
  PhoneHubTray* phone_hub_tray() { return phone_hub_tray_; }
  EcheTray* eche_tray() { return eche_tray_; }

  MouseKeysTray* mouse_keys_tray() { return mouse_keys_tray_; }
  SelectToSpeakTray* select_to_speak_tray() { return select_to_speak_tray_; }
  WmModeButtonTray* wm_mode_button_tray() { return wm_mode_button_tray_; }

  Shelf* shelf() { return shelf_; }

  const std::vector<raw_ptr<TrayBackgroundView, VectorExperimental>>&
  tray_buttons() const {
    return tray_buttons_;
  }

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
  bool OnNativeWidgetActivationChanged(bool active) override;

  // Sets the value for `open_shelf_pod_bubble_`. Note that we only keep track
  // of tray bubble of type `TrayBubbleType::kTrayBubble`.
  void SetOpenShelfPodBubble(TrayBubbleView* open_tray_bubble);

  // TODO(jamescook): Introduce a test API instead of these methods.
  LogoutButtonTray* logout_button_tray_for_testing() {
    return logout_button_tray_;
  }
  VirtualKeyboardTray* virtual_keyboard_tray_for_testing() {
    return virtual_keyboard_tray_;
  }

  CollapseState collapse_state() const { return collapse_state_; }
  void set_collapse_state_for_test(CollapseState state) {
    collapse_state_ = state;
  }

  StatusAreaAnimationController* animation_controller() {
    return animation_controller_.get();
  }

  TrayBubbleView* open_shelf_pod_bubble() { return open_shelf_pod_bubble_; }

 private:
  friend class TrayBackgroundViewTest;
  friend class TrayEventFilterTest;

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
  std::optional<LayoutInputs> layout_inputs_;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // Adds a new tray button to the status area. Implementation is in source
  // file to avoid recursive includes, and function is not used outside of the
  // compilation unit. Template required for a type safe subclass to be
  // returned.
  // Any references to the method outside of this compilation unit will fail
  // linking unless a specialization is declared in status_area_widget.cc.
  template <typename TrayButtonT>
  TrayButtonT* AddTrayButton(std::unique_ptr<TrayButtonT> tray_button);

  // Called when in the collapsed state to calculate and update the visibility
  // of each tray button.
  void CalculateButtonVisibilityForCollapsedState();

  // Move the `stop_recording_button_tray_` to the front so that it's more
  // visible.
  void EnsureTrayOrder();

  // Calculates and returns the appropriate collapse state depending on
  // current conditions.
  CollapseState CalculateCollapseState() const;

  // Update rounded corners for the date tray. The corner behavior for date
  // tray depends on the visibility of the notification center tray.
  void UpdateDateTrayRoundedCorners();

  // Gets the collapse available width based on if the date tray is shown.
  // If `force_collapsible`, returns a fixed width which is not based on the
  // shelf width.
  int GetCollapseAvailableWidth(bool force_collapsible) const;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  const raw_ptr<StatusAreaWidgetDelegate> status_area_widget_delegate_;

  // The active tray bubble that is opened on the display where this status area
  // widget lives.
  raw_ptr<TrayBubbleView> open_shelf_pod_bubble_ = nullptr;

  // All tray items are owned by StatusAreaWidgetDelegate, and destroyed
  // explicitly in a shutdown call in the StatusAreaWidget dtor.
  raw_ptr<StatusAreaOverflowButtonTray, DanglingUntriaged>
      overflow_button_tray_ = nullptr;
  raw_ptr<OverviewButtonTray, DanglingUntriaged> overview_button_tray_ =
      nullptr;
  raw_ptr<DictationButtonTray, DanglingUntriaged> dictation_button_tray_ =
      nullptr;
  raw_ptr<MediaTray, DanglingUntriaged> media_tray_ = nullptr;
  raw_ptr<NotificationCenterTray, DanglingUntriaged> notification_center_tray_ =
      nullptr;
  raw_ptr<DateTray, DanglingUntriaged> date_tray_ = nullptr;
  raw_ptr<UnifiedSystemTray, DanglingUntriaged> unified_system_tray_ = nullptr;
  raw_ptr<LogoutButtonTray, DanglingUntriaged> logout_button_tray_ = nullptr;
  raw_ptr<PaletteTray, DanglingUntriaged> palette_tray_ = nullptr;
  raw_ptr<PhoneHubTray, DanglingUntriaged> phone_hub_tray_ = nullptr;
  raw_ptr<PodsOverflowTray> pods_overflow_tray_ = nullptr;
  raw_ptr<EcheTray, DanglingUntriaged> eche_tray_ = nullptr;
  raw_ptr<VideoConferenceTray, DanglingUntriaged> video_conference_tray_ =
      nullptr;
  raw_ptr<StopRecordingButtonTray, DanglingUntriaged>
      stop_recording_button_tray_ = nullptr;
  raw_ptr<FocusModeTray, DanglingUntriaged> focus_mode_tray_ = nullptr;
  raw_ptr<AnnotationTray, DanglingUntriaged> annotation_tray_ = nullptr;
  raw_ptr<VirtualKeyboardTray, DanglingUntriaged> virtual_keyboard_tray_ =
      nullptr;
  raw_ptr<ImeMenuTray, DanglingUntriaged> ime_menu_tray_ = nullptr;
  raw_ptr<MouseKeysTray, DisableDanglingPtrDetection> mouse_keys_tray_ =
      nullptr;
  raw_ptr<SelectToSpeakTray, DanglingUntriaged> select_to_speak_tray_ = nullptr;
  raw_ptr<HoldingSpaceTray, DanglingUntriaged> holding_space_tray_ = nullptr;
  raw_ptr<WmModeButtonTray, DanglingUntriaged> wm_mode_button_tray_ = nullptr;

  // Vector of the tray buttons above. The ordering is used to determine which
  // tray buttons are hidden when they overflow the available width.
  std::vector<raw_ptr<TrayBackgroundView, VectorExperimental>> tray_buttons_;

  LoginStatus login_status_ = LoginStatus::NOT_LOGGED_IN;

  CollapseState collapse_state_ = CollapseState::NOT_COLLAPSIBLE;

  gfx::Rect target_bounds_;

  raw_ptr<Shelf> shelf_;

  bool initialized_ = false;

  // Owned by `StatusAreaWidget`:
  std::unique_ptr<StatusAreaAnimationController> animation_controller_;

  base::WeakPtrFactory<StatusAreaWidget> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
