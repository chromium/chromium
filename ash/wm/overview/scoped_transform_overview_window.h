// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ui {
class Layer;
}

namespace views {
class Widget;
}

namespace ash {

class ScopedOverviewAnimationSettings;
class WindowSelectorItem;

// Manages a window, and its transient children, in the overview mode. This
// class allows transforming the windows with a helper to determine the best
// fit in certain bounds. The window's state is restored when this object is
// destroyed.
class ASH_EXPORT ScopedTransformOverviewWindow
    : public ui::ImplicitAnimationObserver {
 public:
  // Overview windows have certain properties if their aspect ratio exceedes a
  // threshold. This enum keeps track of which category the window falls into,
  // based on its aspect ratio.
  enum class GridWindowFillMode {
    kNormal = 0,
    kLetterBoxed,
    kPillarBoxed,
  };

  using ScopedAnimationSettings =
      std::vector<std::unique_ptr<ScopedOverviewAnimationSettings>>;

  // Windows whose aspect ratio surpass this (width twice as large as height or
  // vice versa) will be classified as too wide or too tall and will be handled
  // slightly differently in overview mode.
  static constexpr float kExtremeWindowRatioThreshold = 2.f;

  // Calculates and returns an optimal scale ratio. This is only taking into
  // account |size.height()| as the width can vary.
  static float GetItemScale(const gfx::Size& source,
                            const gfx::Size& target,
                            int top_view_inset,
                            int title_height);

  // Returns the transform turning |src_rect| into |dst_rect|.
  static gfx::Transform GetTransformForRect(const gfx::Rect& src_rect,
                                            const gfx::Rect& dst_rect);

  ScopedTransformOverviewWindow(WindowSelectorItem* selector_item,
                                aura::Window* window);
  ~ScopedTransformOverviewWindow() override;

  // Starts an animation sequence which will use animation settings specified by
  // |animation_type|. The |animation_settings| container is populated with
  // scoped entities and the container should be destroyed at the end of the
  // animation sequence.
  //
  // Example:
  //  ScopedTransformOverviewWindow overview_window(window);
  //  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  //  overview_window.BeginScopedAnimation(
  //      OVERVIEW_ANIMATION_SELECTOR_ITEM_SCROLL_CANCEL,
  //      &animation_settings);
  //  // Calls to SetTransform & SetOpacity will use the same animation settings
  //  // until animation_settings is destroyed.
  //  overview_window.SetTransform(root_window, new_transform);
  //  overview_window.SetOpacity(1);
  void BeginScopedAnimation(OverviewAnimationType animation_type,
                            ScopedAnimationSettings* animation_settings);

  // Returns true if this window selector window contains the |target|.
  bool Contains(const aura::Window* target) const;

  // Returns transformed bounds of the overview window. See
  // OverviewUtil::GetTransformedBounds for more details.
  gfx::Rect GetTransformedBounds() const;

  // Returns the kTopViewInset property of |window_| unless there are transient
  // ancestors, in which case returns 0.
  int GetTopInset() const;

  // Restores and animates the managed window to its non overview mode state.
  // If |reset_transform| equals false, the window's transform will not be reset
  // to identity transform when exiting the overview mode. See
  // WindowSelectorItem::RestoreWindow() for details why we need this.
  void RestoreWindow(bool reset_transform, bool use_slide_animation);

  // Informs the ScopedTransformOverviewWindow that the window being watched was
  // destroyed. This resets the internal window pointer.
  void OnWindowDestroyed();

  // Prepares for overview mode by doing any necessary actions before entering.
  void PrepareForOverview();

  // Sets the opacity of the managed windows.
  void SetOpacity(float opacity);

  // Creates/Deletes a mirror window for minimized windows.
  void UpdateMirrorWindowForMinimizedState();

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio). Takes into account a window header that is |top_view_inset|
  // tall in the original window getting replaced by a window caption that is
  // |title_height| tall in the transformed window. If |type_| is not normal,
  // write |window_selector_bounds_|, which would differ than the return bounds.
  gfx::Rect ShrinkRectToFitPreservingAspectRatio(const gfx::Rect& rect,
                                                 const gfx::Rect& bounds,
                                                 int top_view_inset,
                                                 int title_height);

  aura::Window* window() const { return window_; }

  GridWindowFillMode type() const { return type_; }

  base::Optional<gfx::Rect> window_selector_bounds() const {
    return window_selector_bounds_;
  }

  gfx::Rect GetMaskBoundsForTesting() const;

  // Closes the transient root of the window managed by |this|.
  void Close();

  // Returns the window used to show the content in overview mode.
  // For minimized window this will be a window that hosts mirrored layers.
  aura::Window* GetOverviewWindow() const;

  // Ensures that a window is visible by setting its opacity to 1.
  void EnsureVisible();

  // Returns an overview window created for minimized window, or nullptr if it
  // does not exist.
  aura::Window* GetOverviewWindowForMinimizedState() const;

  // Called via WindowSelectorItem from WindowGrid when |window_|'s bounds
  // change. Must be called before PositionWindows in WindowGrid.
  void UpdateWindowDimensionsType();

  // Updates the mask which gives rounded corners on the windows. Shows the mask
  // if |show| is true, otherwise removes it.
  void UpdateMask(bool show);

  // Stop listening to any animations to finish.
  void CancelAnimationsListener();

  // If the original window is minimized, resize |minimized_widget_| to match
  // the bounds of the |window_|.
  void ResizeMinimizedWidgetIfNeeded();

  views::Widget* minimized_widget() { return minimized_widget_.get(); }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  friend class WindowSelectorTest;
  class LayerCachingAndFilteringObserver;
  class WindowMask;

  // Closes the window managed by |this|.
  void CloseWidget();

  void CreateMirrorWindowForMinimizedState();

  // Makes Close() execute synchronously when used in tests.
  static void SetImmediateCloseForTests();

  // A weak pointer to the window selector item that owns the transform window.
  WindowSelectorItem* selector_item_;

  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // Tracks if this window was ignored by the shelf.
  bool ignored_by_shelf_;

  // True if the window has been transformed for overview mode.
  bool overview_started_ = false;

  // The original opacity of the window before entering overview mode.
  float original_opacity_;

  // Specifies how the window is laid out in the grid.
  GridWindowFillMode type_ = GridWindowFillMode::kNormal;

  // Empty if window is of type normal. Contains the bounds the window selector
  // item should be if the window is too wide or too tall.
  base::Optional<gfx::Rect> window_selector_bounds_;

  // A widget that holds the content for the minimized window.
  std::unique_ptr<views::Widget> minimized_widget_;

  // The observers associated with the layers we requested caching render
  // surface and trilinear filtering. The requests will be removed in dtor if
  // the layer has not been destroyed.
  std::vector<std::unique_ptr<LayerCachingAndFilteringObserver>>
      cached_and_filtered_layer_observers_;

  // A mask to be applied on |window_|. This will give |window_| rounded edges
  // while in overview.
  std::unique_ptr<WindowMask> mask_;

  // The original mask layer of the window before entering overview mode.
  ui::Layer* original_mask_layer_ = nullptr;

  base::WeakPtrFactory<ScopedTransformOverviewWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformOverviewWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
