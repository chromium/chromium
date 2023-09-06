// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TILE_GROUP_WINDOW_SPLITTER_H_
#define ASH_WM_TILE_GROUP_WINDOW_SPLITTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class PhantomWindowController;

// The WindowSplitter is responsible for detecting when a window can be split,
// showing the split preview, and performing the actual window splitting.
// It is meant to be used during dragging by a WindowResizer.
class ASH_EXPORT WindowSplitter : public aura::WindowObserver {
 public:
  // The edge position of a window from which to initiate a split.
  enum class SplitPosition {
    kNone,
    kLeft,
    kRight,
    kTop,
    kBottom,
  };

  // Holds bounds of windows after splitting.
  struct SplitWindowBounds {
    gfx::Rect topmost_window_bounds;
    gfx::Rect dragged_window_bounds;
  };

  // Calculates window bounds resulting from window splitting.
  // `topmost_window` is the window to be split.
  // `dragged_window` is the window being dragged over the `topmost_window`.
  // `screen_location` is the screen coordinate of the input event. It must be
  // within the `topmost_window`.
  // Returns nullopt if window can't be split, e.g. the location is not within
  // any trigger area, or the resulting size is smaller than minimum size, etc.
  static absl::optional<SplitWindowBounds> MaybeSplitWindow(
      aura::Window* topmost_window,
      aura::Window* dragged_window,
      const gfx::PointF& screen_location);

  explicit WindowSplitter(aura::Window* dragged_window);
  WindowSplitter(const WindowSplitter&) = delete;
  WindowSplitter& operator=(const WindowSplitter&) = delete;
  ~WindowSplitter() override;

  // Called during drag to determine window splitting activation.
  void UpdateDrag(const gfx::PointF& location_in_screen, bool can_split);

  // Called when drag is completed to apply splitting.
  void CompleteDrag(const gfx::PointF& last_location_in_screen);

  // Disengages window splitting.
  void Disengage();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  const PhantomWindowController* GetPhantomWindowControllerForTesting() const {
    return phantom_window_controller_.get();
  }

 private:
  void ShowPhantomWindow(const gfx::Rect& bounds);

  void MaybeClearDraggedWindow();

  // The window being dragged.
  raw_ptr<aura::Window> dragged_window_ = nullptr;

  // Whether the window can be split upon completing drag.
  bool can_split_window_ = false;

  // Gives a preview of how the window will be split.
  std::unique_ptr<PhantomWindowController> phantom_window_controller_;
};

}  // namespace ash

#endif  // ASH_WM_TILE_GROUP_WINDOW_SPLITTER_H_
