// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TILE_GROUP_WINDOW_SPLITTER_H_
#define ASH_WM_TILE_GROUP_WINDOW_SPLITTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
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
  // The region of a window from which to initiate a split.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Keep this in sync with `WindowSplittingSplitRegion` in
  // tools/metrics/histograms/enums.xml.
  enum class SplitRegion {
    kNone = 0,
    kLeft = 1,
    kRight = 2,
    kTop = 3,
    kBottom = 4,
    kMaxValue = kBottom,
  };

  // The type of action resulting from a completed drag, for logging only.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Keep this in sync with `WindowSplittingDragType` in
  // tools/metrics/histograms/enums.xml.
  enum class DragType {
    kIncomplete = 0,
    kNoSplit = 1,
    kSplit = 2,
    kMaxValue = kSplit,
  };

  // Holds info about windows after splitting.
  struct SplitWindowInfo {
    gfx::Rect topmost_window_bounds;
    gfx::Rect dragged_window_bounds;
    SplitRegion split_region;
  };

  // Calculates window bounds and other info resulting from window splitting.
  // `topmost_window` is the window to be split.
  // `dragged_window` is the window being dragged over the `topmost_window`.
  // `screen_location` is the screen coordinate of the input event. It must be
  // within the `topmost_window`.
  // Returns nullopt if window can't be split, e.g. the location is not within
  // any trigger area, or the resulting size is smaller than minimum size, etc.
  static absl::optional<SplitWindowInfo> MaybeSplitWindow(
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

  void RecordMetricsOnEndDrag();

  DragType GetDragType() const;

  // The window being dragged.
  raw_ptr<aura::Window> dragged_window_ = nullptr;

  // Whether the window can be split upon completing drag.
  bool can_split_window_ = false;

  // Whether the user actually moved enough to be considered a drag.
  bool is_drag_updated_ = false;

  // Whether the drag operation was completed successfully (instead of e.g.
  // cancelled).
  bool is_drag_completed_ = false;

  // The region of a window the split happened, if any;
  SplitRegion completed_split_region_ = SplitRegion::kNone;

  // Gives a preview of how the window will be split.
  std::unique_ptr<PhantomWindowController> phantom_window_controller_;

  // Number of times the phantom window was shown.
  uint32_t phantom_window_shown_count_ = 0;

  // Time ticks when the drag action started.
  const base::TimeTicks drag_start_time_;
};

}  // namespace ash

#endif  // ASH_WM_TILE_GROUP_WINDOW_SPLITTER_H_
