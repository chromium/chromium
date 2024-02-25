// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPH_H_
#define ASH_HUD_DISPLAY_GRAPH_H_

#include <vector>

#include "ash/hud_display/hud_constants.h"
#include "base/containers/ring_buffer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

struct SkPoint;

namespace gfx {
class Canvas;
}

namespace ash {
namespace hud_display {

class Graph {
 public:
  // Graph screen size (that is used during layout) should match (ring buffer
  // size - 1) to prevent scaling, because RingBuffer always keeps one element
  // unused.
  using Data = base::RingBuffer<float, kHUDGraphWidth + 1>;

  enum class Baseline {
    kBaselineBottom,  // Positive values will be drawn from the bottom border
                      // up.
    kBaselineTop,     // Positive values will be drawn from the top border down.
  };

  // Whether to draw the graph as a filled polygon.
  enum class Fill {
    kNone,
    kSolid,
  };

  enum class Style {
    kLines,
    kSkyline,
  };

  // |max_data_points| must be less than the ring buffer size.
  Graph(size_t max_data_points,
        Baseline baseline,
        Fill fill,
        Style style,
        SkColor color);
  ~Graph();

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  // |value| must be normalized to [0,1]. When graphs are drawn stacked,
  // the full stack must be normalized.
  // |unscaled_value| is used to label graph values to the user.
  void AddValue(float value, float unscaled_value);
  void Layout(const gfx::Rect& graph_bounds, const Graph* base);
  void Draw(gfx::Canvas* canvas) const;
  void UpdateLastValue(float value, float unscaled_value);

  const std::vector<SkPoint>& top_path() const { return top_path_; }

  // Returns number of data points displayed on the graph.
  size_t max_data_points() const { return max_data_points_; }

  SkColor color() const { return color_; }

  // Returns value from |unscaled_data_|.
  // |index| is always interpreted as "negative", i.e. "0" - current data, "1"
  // - previous graph data, 2 - two steps "ago". I.e. it's number of graph
  // points from the right graph edge.
  float GetUnscaledValueAt(size_t index) const;

  // Returns true if |data_| is populated at the given index.
  // |index| is always interpreted as "negative", i.e. "0" - current data, "1"
  // - previous graph data, 2 - two steps ago. I.e. it's number of graph
  // points from the right graph edge.
  bool IsFilledIndex(size_t index) const;

  // Reset the data.
  void Reset();

#if !defined(NDEBUG)
  // Returns string representation os this object for debug.
  std::string DebugDump(const std::string& name) const;
#endif

 private:
  const Baseline baseline_;
  const Fill fill_;
  const Style style_;
  const SkColor color_;

  // Result of last Layout() call.
  gfx::Rect graph_bounds_;

  // Paths are measured from the top left corner.
  // Partial graph is assumed to be right-justified.
  // For kBaselineBottom |top_path_| has y values that are less than
  // |bottom_path_|. (And opposite for the kBaselineTop.)
  // Paths are calculated by Layout() from the |data_|.
  std::vector<SkPoint> top_path_;
  std::vector<SkPoint> bottom_path_;
  // Bottom path style should follow base graph style.
  Style bottom_path_style_ = Style::kLines;

  Data data_;
  Data unscaled_data_;
  size_t max_data_points_ = 0;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPH_H_
