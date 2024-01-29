// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_WIDGET_H_
#define ASH_SHELF_HOTSEAT_WIDGET_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/shelf_component.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"

namespace aura {
class ScopedWindowTargeter;
}

namespace ash {
class FocusCycler;
class ScrollableShelfView;
class Shelf;
class ShelfView;
class HotseatTransitionAnimator;

// The hotseat widget is part of the shelf and hosts app shortcuts.
class ASH_EXPORT HotseatWidget : public ShelfComponent,
                                 public ShelfConfig::Observer,
                                 public views::Widget {
 public:
  // Defines the hotseat transition types.
  enum class StateTransition {
    // Hotseat state transits between kShownHomeLauncher and kExtended.
    kHomeLauncherAndExtended,

    // Hotseat state transits between kShownHomeLauncher and kHidden.
    kHomeLauncherAndHidden,

    // Hotseat state transits between kHidden and kExtended.
    kHiddenAndExtended,

    kOther
  };

  // Scoped class to notify HotseatWidget of hotseat state transition in
  // progress. We should not calculate the state transition simply in
  // HotseatWidget::SetState(). Otherwise it is hard to reset when the
  // transition completes.
  class ScopedInStateTransition {
   public:
    ScopedInStateTransition(HotseatWidget* hotseat_widget,
                            HotseatState old_state,
                            HotseatState target_state);
    ~ScopedInStateTransition();

    ScopedInStateTransition(const ScopedInStateTransition& rhs) = delete;
    ScopedInStateTransition& operator=(const ScopedInStateTransition& rhs) =
        delete;

   private:
    raw_ptr<HotseatWidget> hotseat_widget_ = nullptr;
  };

  HotseatWidget();

  HotseatWidget(const HotseatWidget&) = delete;
  HotseatWidget& operator=(const HotseatWidget&) = delete;

  ~HotseatWidget() override;

  // Returns whether the hotseat background should be shown.
  static bool ShouldShowHotseatBackground();

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container, Shelf* shelf);

  // Initializes the animation metrics reporter responsible for recording
  // animation performance during hotseat state changes, and attaches
  // |delegate_view_| as an observer.
  void OnHotseatTransitionAnimatorCreated(HotseatTransitionAnimator* animator);

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // Whether the widget is in the extended position.
  bool IsExtended() const;

  // Finds the first or last focusable app shortcut and focuses it.
  void FocusFirstOrLastFocusableChild(bool last);

  // Notifies children of tablet mode state changes.
  void OnTabletModeChanged();

  // Returns the target opacity for the shelf view given current conditions.
  float CalculateShelfViewOpacity() const;

  // Updates the bounds of the translucent background which functions as the
  // hotseat background.
  void UpdateTranslucentBackground();

  // Calculates the hotseat y position for |hotseat_target_state| in screen
  // coordinates.
  int CalculateHotseatYInScreen(HotseatState hotseat_target_state) const;

  // Calculates the hotseat target bounds's size for the given target state.
  gfx::Size CalculateTargetBoundsSize(HotseatState hotseat_target_state) const;

  // Calculates space available for app bar if shown inline with shelf.
  gfx::Size CalculateInlineAppBarSize() const;

  // Takes insets to reserve when calculating bounds.
  void ReserveSpaceForAdjacentWidgets(const gfx::Insets& space);

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // TODO(manucornet): Remove this method once all the hotseat layout
  // code has moved to this class.
  void set_target_bounds(gfx::Rect target_bounds) {
    target_bounds_ = target_bounds;
  }

  gfx::Size GetTranslucentBackgroundSize() const;

  // Sets the focus cycler and adds the hotseat to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);

  bool IsShowingShelfMenu() const;

  // Whether the event is located in the hotseat area containing shelf apps.
  bool EventTargetsShelfView(const ui::LocatedEvent& event) const;

  ShelfView* GetShelfView();
  const ShelfView* GetShelfView() const;

  // Returns the hotseat height (or width for side shelf).
  int GetHotseatSize() const;

  // Returns the drag distance required to fully show the hotseat widget from
  // the hidden state.
  int GetHotseatFullDragAmount() const;

  // Updates the target hotseat density, if needed. Returns whether
  // |target_hotseat_density_| has changed after calling this method.
  bool UpdateTargetHotseatDensityIfNeeded();

  // Returns the background blur of the |translucent_background_|, for tests.
  int GetHotseatBackgroundBlurForTest() const;

  // Returns whether the translucent background is visible, for tests.
  bool GetIsTranslucentBackgroundVisibleForTest() const;

  metrics_util::ReportCallback GetTranslucentBackgroundReportCallback();

  void SetState(HotseatState state);
  HotseatState state() const { return state_; }

  ScrollableShelfView* scrollable_shelf_view() {
    return scrollable_shelf_view_;
  }

  const ScrollableShelfView* scrollable_shelf_view() const {
    return scrollable_shelf_view_;
  }

  // Whether the widget is in the extended position because of a direct
  // manual user intervention (dragging the hotseat into its extended state).
  // This will return |false| after any visible change in the shelf
  // configuration.
  bool is_manually_extended() const { return is_manually_extended_; }

  void set_manually_extended(bool value) { is_manually_extended_ = value; }

  HotseatDensity target_hotseat_density() const {
    return target_hotseat_density_;
  }

  // The layer that should be used to animate hotseat bounds while showing the
  // home to overview contextual nudge.
  ui::Layer* GetLayerForNudgeAnimation();

  // Returns if the shelf is going to be overflown.
  bool CalculateShelfOverflow(bool use_target_bounds) const;

 private:
  class DelegateView;

  struct LayoutInputs {
    gfx::Rect bounds;
    float shelf_view_opacity = 0.0f;
    bool is_active_session_state = false;
    gfx::Insets reserved_space_;

    bool operator==(const LayoutInputs& other) const {
      return bounds == other.bounds &&
             shelf_view_opacity == other.shelf_view_opacity &&
             is_active_session_state == other.is_active_session_state &&
             reserved_space_ == other.reserved_space_;
    }
  };

  // Collects the inputs for layout.
  LayoutInputs GetLayoutInputs() const;

  // May update the hotseat widget's target in account of app scaling.
  void MaybeAdjustTargetBoundsForAppScaling(HotseatState hotseat_target_state);

  // Calculates the target hotseat density.
  HotseatDensity CalculateTargetHotseatDensity() const;

  // Animates the hotseat to the target opacity/bounds.
  void LayoutHotseatByAnimation(double target_opacity,
                                const gfx::Rect& target_bounds);

  // Start the animation designed specifically for |state_transition|.
  void StartHotseatTransitionAnimation(StateTransition state_transition,
                                       double target_opacity,
                                       const gfx::Rect& target_bounds);

  // Starts the default bounds/opacity animation.
  void StartNormalBoundsAnimation(double target_opacity,
                                  const gfx::Rect& target_bounds);

  // The set of inputs that impact this widget's layout. The assumption is that
  // this widget needs a relayout if, and only if, one or more of these has
  // changed.
  std::optional<LayoutInputs> layout_inputs_;

  gfx::Rect target_bounds_;

  // The size that |target_bounds_| would have in kShownHomeLauncher state.
  // Used to calculate hotseat density state.
  gfx::Size target_size_for_shown_state_;

  HotseatState state_ = HotseatState::kNone;

  // Indicates the type of the hotseat state transition in progress.
  std::optional<StateTransition> state_transition_in_progress_;

  raw_ptr<Shelf> shelf_ = nullptr;

  // View containing the shelf items within an active user session. Owned by
  // the views hierarchy.
  raw_ptr<ScrollableShelfView, DanglingUntriaged> scrollable_shelf_view_ =
      nullptr;

  // The contents view of this widget. Contains |shelf_view_| and the background
  // of the hotseat.
  raw_ptr<DelegateView> delegate_view_ = nullptr;

  // Whether the widget is currently extended because the user has manually
  // dragged it. This will be reset with any visible shelf configuration change.
  bool is_manually_extended_ = false;

  // Indicates the target hotseat density. When app scaling feature is enabled,
  // hotseat may become denser if there is insufficient view space to
  // accommodate all app icons without scrolling.
  HotseatDensity target_hotseat_density_ = HotseatDensity::kNormal;

  // The window targeter installed on the hotseat. Filters out events which land
  // on the non visible portion of the hotseat, or events that reach the hotseat
  // during an animation.
  std::unique_ptr<aura::ScopedWindowTargeter> hotseat_window_targeter_;

  // Space reserved by other widgets to exclude when calculating bounds and hit
  // area.
  gfx::Insets reserved_space_;
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_WIDGET_H_
