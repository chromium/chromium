// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_VIEW_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/gestures/wm_fling_handler.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Label;
class View;
}  // namespace views

namespace ash {
class WindowMiniViewBase;
class LabelSliderButton;
class SystemShadow;
class TabSlider;
class WindowCycleItemView;

// A view that shows a collection of windows the user can cycle through.
class ASH_EXPORT WindowCycleView : public views::WidgetDelegateView,
                                   public ui::ImplicitAnimationObserver,
                                   public views::ViewObserver {
  METADATA_HEADER(WindowCycleView, views::WidgetDelegateView)

 public:
  using WindowList = std::vector<raw_ptr<aura::Window, VectorExperimental>>;

  // Horizontal padding between the alt-tab bandshield and the window
  // previews.
  static constexpr int kInsideBorderHorizontalPaddingDp = 64;

  WindowCycleView(aura::Window* root_window,
                  const WindowList& windows,
                  const bool same_app_only);
  WindowCycleView(const WindowCycleView&) = delete;
  WindowCycleView& operator=(const WindowCycleView&) = delete;
  ~WindowCycleView() override;

  aura::Window* target_window() const { return target_window_; }

  // Scales the window cycle view by scaling its clip rect. If the widget is
  // growing, the widget's bounds are set to `screen_bounds` immediately then
  // its clipping rect is scaled. If the widget is shrinking, the widget's
  // cliping rect is scaled first then the widget's bounds are set to
  // |screen_bounds| upon completion/interruption of the clipping rect's
  // animation.
  void ScaleCycleView(const gfx::Rect& screen_bounds);

  // Returns the target bounds of |this|, that is its preferred size clamped to
  // the root window's bounds.
  gfx::Rect GetTargetBounds() const;

  // Recreates the `WindowCycleView` with the given `windows`.
  void UpdateWindows(const WindowList& windows);

  // Fades the `WindowCycleView` in.
  void FadeInLayer();

  // Scrolls the `WindowCycleView` to `target`.
  void ScrollToWindow(aura::Window* target);

  // Refreshes the `target_window_` with the `new_target`. Updates the focus
  // state of the focus ring by hiding the focus ring on the previously
  // focused item and painting the focus ring on the currently focused item.
  // The focus target will be a single `WindowCycleItemView` for free-form
  // window and a `GroupContainerCycleView` for snap group.
  void SetTargetWindow(aura::Window* new_target);

  // Removes the `destroying_window`'s respective `WindowCycleItemView` and sets
  // `new_target` as the new `target_window_`.
  void HandleWindowDestruction(aura::Window* destroying_window,
                               aura::Window* new_target);

  // Clears all state and removes all child views.
  void DestroyContents();

  // Horizontally translates the `WindowCycleView` by `delta_x`.
  void Drag(float delta_x);

  // Creates a `WmFlingHandler` which will horizontally translate the
  // `WindowCycleView`.
  void StartFling(float velocity_x);

  // Called on each fling step, updates `horizontal_distance_dragged_` by
  // `offset`.
  bool OnFlingStep(float offset);

  // Called when a fling ends, cleans up fling state.
  void OnFlingEnd();

  // Sets whether the `tab_slider_` is focused.
  void SetFocusTabSlider(bool focus);

  // Returns whether the `tab_slider_` is focused.
  bool IsTabSliderFocused() const;

  // Returns the corresponding window for the `WindowCycleItemView` located at
  // `screen_point`.
  aura::Window* GetWindowAtPoint(const gfx::Point& screen_point);

  // Called when the alt-tab mode is changed, notifying the
  // `tab_slider_container_` of the change.
  void OnModePrefsChanged();

  // Returns whether or not the given `screen_point` is located in tab slider
  // container.
  bool IsEventInTabSliderContainer(const gfx::Point& screen_point) const;

  // Returns the maximum width of the cycle view.
  int CalculateMaxWidth() const;

  // views::WidgetDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  const views::View* mirror_container_for_testing() const {
    return mirror_container_;
  }

  const std::vector<raw_ptr<WindowMiniViewBase, VectorExperimental>>&
  cycle_views_for_testing() const {
    return cycle_views_;
  }

 protected:
  // ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

 private:
  friend class WindowCycleListTestApi;

  // Returns a bound of alt-tab content container, which represents the mirror
  // container when there is at least one window and represents no-recent-items
  // label when there is no window to be shown.
  gfx::Rect GetContentContainerBounds() const;

  // Returns the corresponding `WindowMiniViewBase` for the given `window` or
  // nullptr if not found.
  WindowMiniViewBase* GetCycleViewForWindow(aura::Window* window) const;

  // The root window that `this` resides on.
  const raw_ptr<aura::Window> root_window_;

  // True if the `this` is built for same app cycling.
  const bool same_app_only_;

  // Constructed as the child views of `mirror_container` and used for window
  // cycling.
  std::vector<raw_ptr<WindowMiniViewBase, VectorExperimental>> cycle_views_;

  // A container that hosts and lays out all the `WindowMiniViewBase`s.
  raw_ptr<views::BoxLayoutView> mirror_container_ = nullptr;

  // Tells users that there are no app windows on the active desk. It only shows
  // when there're more than 1 desk.
  raw_ptr<views::Label> no_recent_items_label_ = nullptr;

  // The `tab_slider_` only shows when there're more than 1 desk. It contains
  // `all_desks_tab_slider_button_` and `current_desk_tab_slider_button_` which
  // user can tab through or toggle between.
  raw_ptr<TabSlider> tab_slider_ = nullptr;
  raw_ptr<LabelSliderButton> all_desks_tab_slider_button_ = nullptr;
  raw_ptr<LabelSliderButton> current_desk_tab_slider_button_ = nullptr;

  // The |target_window_| is the window that has the focus ring. When the user
  // completes cycling the |target_window_| is activated.
  raw_ptr<aura::Window> target_window_ = nullptr;

  // The |current_window_| is the window that the window cycle list uses to
  // determine the layout and positioning of the list's items. If this window's
  // preview can equally divide the list it is centered, otherwise it is
  // off-center.
  raw_ptr<aura::Window> current_window_ = nullptr;

  // Used when the widget bounds update should be deferred during the cycle
  // view's scaling animation..
  bool defer_widget_bounds_update_ = false;

  // List which contains items which have been created but have some of their
  // performance heavy elements not created yet. These elements will be created
  // once onscreen to improve fade in performance, then removed from this set.
  std::vector<raw_ptr<WindowMiniViewBase, VectorExperimental>>
      no_previews_list_;

  // Tracks the distance that a user has dragged, offsetting the
  // |mirror_container_|. This should be reset only when a user cycles the
  // window cycle list or when the user switches alt-tab modes.
  float horizontal_distance_dragged_ = 0.f;

  // Fling handler of the current active fling. Nullptr while a fling is not
  // active.
  std::unique_ptr<WmFlingHandler> fling_handler_;

  std::unique_ptr<SystemShadow> shadow_;

  // Indicates whether the selector view on `tab_slider_` is focused or not. We
  // need to manually schedule paint for the focus ring since the tab slider
  // buttons are not focusable.
  bool is_tab_slider_focused_ = false;

  // True once `DestroyContents` is called. Used to prevent `Layout` from being
  // called once all the child views have been removed. See
  // https://crbug.com/1223302 for more details.
  bool is_destroying_ = false;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_VIEW_H_
