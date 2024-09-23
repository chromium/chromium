// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/raster_scale/raster_scale_layer_observer.h"
#include "ash/wm/scoped_layer_tree_synchronizer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace aura {
class ScopedWindowEventTargetingBlocker;
class Window;
}  // namespace aura

namespace ash {
class OverviewItem;
class ScopedOverviewAnimationSettings;
class ScopedOverviewHideWindows;

// Manages a window, and its transient children, in the overview mode. This
// class allows transforming the windows with a helper to determine the best
// fit in certain bounds. The window's state is restored when this object is
// destroyed.
class ASH_EXPORT ScopedOverviewTransformWindow
    : public aura::client::TransientWindowClientObserver,
      public aura::WindowObserver {
 public:
  using ScopedAnimationSettings =
      std::vector<std::unique_ptr<ScopedOverviewAnimationSettings>>;

  // Calculates and returns an optimal scale ratio. This is only taking into
  // account height as the width can vary.
  static float GetItemScale(int source_height,
                            int target_height,
                            int top_view_inset,
                            int title_height);

  ScopedOverviewTransformWindow(OverviewItem* overview_item,
                                aura::Window* window);
  ScopedOverviewTransformWindow(const ScopedOverviewTransformWindow&) = delete;
  ScopedOverviewTransformWindow& operator=(
      const ScopedOverviewTransformWindow&) = delete;
  ~ScopedOverviewTransformWindow() override;

  aura::Window* window() const { return window_; }

  bool is_restoring() const { return is_restoring_; }

  OverviewItemFillMode fill_mode() const { return fill_mode_; }

  // Starts an animation sequence which will use animation settings specified by
  // |animation_type|. The |animation_settings| container is populated with
  // scoped entities and the container should be destroyed at the end of the
  // animation sequence.
  //
  // Example:
  //  ScopedOverviewTransformWindow overview_window(window);
  //  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  //  overview_window.BeginScopedAnimation(
  //      OVERVIEW_ANIMATION_RESTORE_WINDOW, &animation_settings);
  //  // Calls to SetTransform & SetOpacity will use the same animation settings
  //  // until animation_settings is destroyed.
  //  SetTransform(root_window, new_transform) in `overview_utils.h`;
  //  overview_window.SetOpacity(1);
  void BeginScopedAnimation(OverviewAnimationType animation_type,
                            ScopedAnimationSettings* animation_settings);

  // Returns true if this overview window contains the |target|.
  bool Contains(const aura::Window* target) const;

  // Returns transformed bounds of the overview window.
  gfx::RectF GetTransformedBounds() const;

  // Returns the kTopViewInset property of |window_| unless there are transient
  // ancestors, in which case returns 0.
  int GetTopInset() const;

  // Restores and animates the managed window to its non overview mode state. If
  // `animate` is false, the window will just be restored and not animated. If
  // `reset_transform` equals false, the window's transform will not be reset to
  // identity transform when exiting the overview mode. See
  // `OverviewItem::RestoreWindow()` for details why we need this.
  void RestoreWindow(bool reset_transform, bool animate);

  // Prepares for overview mode by doing any necessary actions before entering.
  void PrepareForOverview();

  // Sets the opacity of the managed windows.
  void SetOpacity(float opacity);

  // Apply clipping on the `window_`. Clip always starts on the origin of
  // `window_`'s layer.
  void SetClipping(const gfx::Rect& clip_rect);

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio). Takes into account a window header that is |top_view_inset|
  // tall in the original window getting replaced by a window caption that is
  // |title_height| tall in the transformed window.
  gfx::RectF ShrinkRectToFitPreservingAspectRatio(const gfx::RectF& rect,
                                                  const gfx::RectF& bounds,
                                                  int top_view_inset,
                                                  int title_height) const;

  // Returns the window used to show the content in overview mode.
  // For minimized window this will be a window that hosts mirrored layers.
  aura::Window* GetOverviewWindow();

  // Closes the transient root of the window managed by |this|.
  void Close();

  bool IsMinimizedOrTucked() const;

  // Ensures that a window is visible by setting its opacity to 1.
  void EnsureVisible();

  // Called via OverviewItem from OverviewGrid when |window_|'s bounds
  // change. Must be called before PositionWindows in OverviewGrid.
  void UpdateOverviewItemFillMode();

  // Updates the rounded corners on `window_` and its transient hierarchy (if
  // needed).
  void UpdateRoundedCorners(bool show);

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // If true, makes `CloseWidget()` execute synchronously when used in tests.
  static void SetImmediateCloseForTests(bool immediate);

 private:
  friend class OverviewTestBase;
  FRIEND_TEST_ALL_PREFIXES(OverviewSessionTest, CloseAnimationShadow);
  class LayerCachingAndFilteringObserver;

  // Closes the window managed by |this|.
  void CloseWidget();

  // Adds transient windows that should be hidden to the hidden window list. The
  // windows are hidden in overview mode and the visibility of the windows is
  // recovered after overview mode.
  void AddHiddenTransientWindows(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
          transient_windows);

  // A weak pointer to the overview item that owns |this|. Guaranteed to be not
  // null for the lifetime of |this|.
  raw_ptr<OverviewItem> overview_item_;

  // A weak pointer to the real window in the overview.
  raw_ptr<aura::Window> window_;

  // True during the process of `RestoreWindow()`. This prevents redundant
  // cyclic calls to `OverviewItem::SetBounds()`, which may happen when
  // `ScopedOverviewTransformWindow::OnWindowBoundsChanged()` is triggered
  // during the restore see http://b/311255082 for an example.
  bool is_restoring_ = false;

  // The original opacity of the window before entering overview mode.
  float original_opacity_;

  // Specifies how the window is laid out in the grid.
  OverviewItemFillMode fill_mode_ = OverviewItemFillMode::kNormal;

  // The observers associated with the layers we requested caching render
  // surface and trilinear filtering. The requests will be removed in dtor if
  // the layer has not been destroyed.
  std::vector<std::unique_ptr<LayerCachingAndFilteringObserver>>
      cached_and_filtered_layer_observers_;

  // For the duration of this object |window_| and its transient childrens'
  // event targeting policy will be sent to NONE. Store the originals so we can
  // change it back when destroying |this|.
  base::flat_map<aura::Window*,
                 std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>>
      event_targeting_blocker_map_;

  // The original clipping on the layer of the window before entering overview
  // mode.
  gfx::Rect original_clip_rect_;

  // Removes clipping on `window_` during destruction in the case it was not
  // removed in `RestoreWindw()`. See destructor for more information.
  bool reset_clip_on_shutdown_ = true;

  std::unique_ptr<ScopedOverviewHideWindows> hidden_transient_children_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  std::unique_ptr<ScopedWindowTreeSynchronizer> window_tree_synchronizer_;

  // While the transform window exists, apply dynamic raster scale to the
  // underlying window.
  std::optional<ScopedRasterScaleLayerObserverLock> raster_scale_observer_lock_;

  base::WeakPtrFactory<ScopedOverviewTransformWindow> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_
