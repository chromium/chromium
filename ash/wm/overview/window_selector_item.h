// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ui {
class Shadow;
}  // namespace ui

namespace ash {

class WindowGrid;

// This class represents an item in overview mode.
class ASH_EXPORT WindowSelectorItem : public views::ButtonListener,
                                      public aura::WindowObserver,
                                      public ui::ImplicitAnimationObserver {
 public:
  // An image button with a close window icon.
  class OverviewCloseButton : public views::ImageButton {
   public:
    explicit OverviewCloseButton(views::ButtonListener* listener);
    ~OverviewCloseButton() override;

    // Resets the listener so that the listener can go out of scope.
    void ResetListener() { listener_ = nullptr; }

   protected:
    // views::Button:
    std::unique_ptr<views::InkDrop> CreateInkDrop() override;
    std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
    std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
        const override;
    std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

   private:
    int GetInkDropRadius() const;

    DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
  };

  WindowSelectorItem(aura::Window* window,
                     WindowSelector* window_selector,
                     WindowGrid* window_grid);
  ~WindowSelectorItem() override;

  aura::Window* GetWindow();

  // Returns the native window of the |transformed_window_|'s minimized widget
  // if the original window is in minimized state, or the original window
  // otherwise.
  aura::Window* GetWindowForStacking();

  // Returns the root window on which this item is shown.
  aura::Window* root_window() { return root_window_; }

  // Returns true if |target| is contained in this WindowSelectorItem.
  bool Contains(const aura::Window* target) const;

  // Restores and animates the managed window to its non overview mode state.
  // If |reset_transform| equals false, the window's transform will not be
  // reset to identity transform when exiting overview mode. It's needed when
  // dragging an Arc app window in overview mode to put it in split screen. In
  // this case the restore of its transform needs to be deferred until the Arc
  // app window is snapped successfully, otherwise the animation will look very
  // ugly (the Arc app window enlarges itself to maximized window bounds and
  // then shrinks to its snapped window bounds). Note if the window's transform
  // is not reset here, it must be reset by someone else at some point.
  void RestoreWindow(bool reset_transform);

  // Ensures that a possibly minimized window becomes visible after restore.
  void EnsureVisible();

  // Restores stacking of window captions above the windows, then fades out.
  void Shutdown();

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  void PrepareForOverview();

  // Calculates and returns an optimal scale ratio. With MD this is only
  // taking into account |size.height()| as the width can vary. Without MD this
  // returns the scale that allows the item to fully fit within |size|.
  float GetItemScale(const gfx::Size& size);

  // Returns the union of the original target bounds of all transformed windows
  // managed by |this| item, i.e. all regular (normal or panel transient
  // descendants of the window returned by GetWindow()).
  gfx::Rect GetTargetBoundsInScreen() const;

  // Returns the transformed bound of |transform_window_|.
  gfx::Rect GetTransformedBounds() const;

  // Sets the bounds of this window selector item to |target_bounds| in the
  // |root_window_| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(const gfx::Rect& target_bounds,
                 OverviewAnimationType animation_type);

  // Activates or deactivates selection depending on |selected|.
  // In selected state the item's caption is shown transparent and blends with
  // the selection widget.
  void set_selected(bool selected) { selected_ = selected; }

  // Sends an accessibility event indicating that this window became selected
  // so that it's highlighted and announced if accessibility features are
  // enabled.
  void SendAccessibleSelectionEvent();

  // Slides the item up or down and then closes the associated window. Used by
  // overview swipe to close.
  void AnimateAndCloseWindow(bool up);

  // Closes |transform_window_|.
  void CloseWindow();

  // Called when the window is minimized or unminimized.
  void OnMinimizedStateChanged();

  // Shows the cannot snap warning if currently in splitview, and the associated
  // window cannot be snapped.
  void UpdateCannotSnapWarningVisibility();

  // Called when a WindowSelectorItem on any grid is dragged. Hides the close
  // button when a drag is started, and reshows it when a drag is finished.
  // Additionally hides the title and window icon if |item| is this.
  void OnSelectorItemDragStarted(WindowSelectorItem* item);
  void OnSelectorItemDragEnded();

  ScopedTransformOverviewWindow::GridWindowFillMode GetWindowDimensionsType()
      const;

  // Recalculates the window dimensions type of |transform_window_|. Called when
  // |window_|'s bounds change.
  void UpdateWindowDimensionsType();

  // Enable or disable the backdrop. If the window is not letter or pillar
  // boxed, nothing will happen.
  void EnableBackdropIfNeeded();
  void DisableBackdrop();
  void UpdateBackdropBounds();

  // TODO(minch): Do not actually scale up the item to get the bounds.
  // http://crbug.com/876567.
  // Returns the bounds of the selected item, which will be scaled up a little
  // bit and header view will be hidden after being selected. Note, the item
  // will be restored back after scaled up.
  gfx::Rect GetBoundsOfSelectedItem();

  // Increases the bounds of the dragged item.
  void ScaleUpSelectedItem(OverviewAnimationType animation_type);

  // Sets if the item is dimmed in the overview. Changing the value will also
  // change the visibility of the transform windows.
  void SetDimmed(bool dimmed);
  bool dimmed() const { return dimmed_; }

  const gfx::Rect& target_bounds() const { return target_bounds_; }

  // Stacks the |item_widget_| in the correct place. |item_widget_| may be
  // initially stacked in the wrong place due to animation or if it is a
  // minimized window, the overview minimized widget is not available on
  // |item_widget_|'s creation.
  void RestackItemWidget();

  // Shift the window item up and then animates it to its original spot. Used
  // to transition from the home launcher.
  void SlideWindowIn();

  // Translate and fade the window (or minimized widget) and |item_widget_|. It
  // should remain in the same spot relative to the grids origin, which is given
  // by |new_grid_y|.
  void UpdateYPositionAndOpacity(
      int new_grid_y,
      float opacity,
      WindowSelector::UpdateAnimationSettingsCallback callback);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Handle the mouse/gesture event and facilitate dragging the item.
  void HandlePressEvent(const gfx::Point& location_in_screen);
  void HandleReleaseEvent(const gfx::Point& location_in_screen);
  void HandleDragEvent(const gfx::Point& location_in_screen);
  void HandleLongPressEvent(const gfx::Point& location_in_screen);
  void HandleFlingStartEvent(const gfx::Point& location_in_screen,
                             float velocity_x,
                             float velocity_y);
  void ActivateDraggedWindow();
  void ResetDraggedWindowGesture();

  // Checks if this item is current being dragged.
  bool IsDragItem();

  // Called after a positioning transform animation ends. Checks to see if the
  // animation was triggered by a drag end event. If so, inserts the window back
  // to its original stacking order so that the order of windows is the same as
  // when entering overview.
  void OnDragAnimationCompleted();

  // Sets the bounds of the window shadow. If |bounds_in_screen| is nullopt,
  // the shadow is hidden.
  void SetShadowBounds(base::Optional<gfx::Rect> bounds_in_screen);

  // Show or hide the mask and shadow on this window item.
  void UpdateMaskAndShadow(bool show);

  // Changes the opacity of all the windows the item owns.
  void SetOpacity(float opacity);
  float GetOpacity();

  void set_should_animate_when_entering(bool should_animate) {
    should_animate_when_entering_ = should_animate;
  }
  bool should_animate_when_entering() const {
    return should_animate_when_entering_;
  }

  void set_should_animate_when_exiting(bool should_animate) {
    should_animate_when_exiting_ = should_animate;
  }
  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  OverviewAnimationType GetExitOverviewAnimationType();
  OverviewAnimationType GetExitTransformAnimationType();

  WindowGrid* window_grid() { return window_grid_; }

  void set_should_restack_on_animation_end(bool val) {
    should_restack_on_animation_end_ = val;
  }

  bool animating_to_close() const { return animating_to_close_; }
  void set_animating_to_close(bool val) { animating_to_close_ = val; }

  float GetCloseButtonVisibilityForTesting() const;
  float GetTitlebarOpacityForTesting() const;
  gfx::Rect GetShadowBoundsForTesting();

 private:
  class CaptionContainerView;
  class RoundedContainerView;
  friend class WindowSelectorTest;
  FRIEND_TEST_ALL_PREFIXES(SplitViewWindowSelectorTest,
                           OverviewUnsnappableIndicatorVisibility);

  // The different ways the overview header can fade in and be laid out.
  // TODO(sammiequon): See if we can combine this with
  // WindowSelector::OverviewTransition.
  enum class HeaderFadeInMode {
    // Used when entering overview mode, to fade in the header background color.
    kEnter,
    // Used when the overview header bounds change for the first time to
    // skip animating.
    kFirstUpdate,
    // Used when the overview header bounds change, to animate or move the
    // header to the desired bounds.
    kUpdate,
  };

  // Sets the bounds of this selector's items to |target_bounds| in
  // |root_window_|. The bounds change will be animated as specified
  // by |animation_type|.
  void SetItemBounds(const gfx::Rect& target_bounds,
                     OverviewAnimationType animation_type);

  // Creates the window label.
  void CreateWindowLabel(const base::string16& title);

  // Updates the close button's and title label's bounds. Any change in bounds
  // will be animated from the current bounds to the new bounds as per the
  // |animation_type|. |mode| allows distinguishing the first time update which
  // allows setting the initial bounds properly or exiting overview to fade out
  // gradually.
  void UpdateHeaderLayout(HeaderFadeInMode mode,
                          OverviewAnimationType animation_type);

  // Animates opacity of the |transform_window_| and its caption to |opacity|
  // using |animation_type|.
  void AnimateOpacity(float opacity, OverviewAnimationType animation_type);

  // Updates the accessibility name to match the window title.
  void UpdateAccessibilityName();

  // Allows a test to directly set animation state.
  gfx::SlideAnimation* GetBackgroundViewAnimation();

  aura::Window* GetOverviewWindowForMinimizedStateForTest();

  // Called before dragging. Scales up the window a little bit to indicate its
  // selection and stacks the window at the top of the Z order in order to keep
  // it visible while dragging around.
  void StartDrag();

  // True if the item is being shown in the overview, false if it's being
  // filtered.
  bool dimmed_ = false;

  // The root window this item is being displayed on.
  aura::Window* root_window_;

  // The contained Window's wrapper.
  ScopedTransformOverviewWindow transform_window_;

  // The target bounds this selector item is fit within.
  gfx::Rect target_bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_ = false;

  // True when |this| item is visually selected. Item header is made transparent
  // when the item is selected.
  bool selected_ = false;

  // A widget that covers the |transform_window_|. The widget has
  // |caption_container_view_| as its contents view. The widget is backed by a
  // NOT_DRAWN layer since most of its surface is transparent.
  std::unique_ptr<views::Widget> item_widget_;

  // A widget that is available if the window is letter or pillar boxed. It is
  // stacked below |transform_window_|'s window. This is nullptr when the window
  // is normal boxed.
  std::unique_ptr<views::Widget> backdrop_widget_;

  // Container view that owns a Button view covering the |transform_window_|.
  // That button serves as an event shield to receive all events such as clicks
  // targeting the |transform_window_| or the overview header above the window.
  // The shield button owns a header view which owns |label_view_|
  // and |close_button_|.
  CaptionContainerView* caption_container_view_ = nullptr;

  // A View for the text label above the window owned by the a header view in
  // |caption_container_view_|.
  views::Label* label_view_ = nullptr;

  // A View for the text label in the center of the window warning users that
  // this window cannot be snapped for splitview. Owned by a container in
  // |caption_container_view_|.
  views::Label* cannot_snap_label_view_ = nullptr;

  // A close button for the window in this item owned by a header view in
  // |caption_container_view_|.
  OverviewCloseButton* close_button_ = nullptr;

  // Pointer to the WindowSelector that owns the WindowGrid containing |this|.
  // Guaranteed to be non-null for the lifetime of |this|.
  WindowSelector* window_selector_;

  // Pointer to the WindowGrid that contains |this|. Guaranteed to be non-null
  // for the lifetime of |this|.
  WindowGrid* window_grid_;

  // True if the contained window should animate during the entering animation.
  bool should_animate_when_entering_ = true;

  // True if the contained window should animate during the exiting animation.
  bool should_animate_when_exiting_ = true;

  // True if after an animation, we need to reorder the stacking order of the
  // widgets.
  bool should_restack_on_animation_end_ = false;

  // True if the windows are still alive so they can have a closing animation.
  // These windows should not be used in calculations for
  // WindowGrid::PositionWindows.
  bool animating_to_close_ = false;

  // The shadow around the overview window. Shadows the original window, not
  // |item_widget_|. Done here instead of on the original window because of the
  // rounded edges mask applied on entering overview window.
  std::unique_ptr<ui::Shadow> shadow_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorItem);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
