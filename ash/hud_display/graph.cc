// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/graph.h"

#include <algorithm>
#include <limits>
#include <sstream>

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {
namespace hud_display {

Graph::Graph(size_t max_data_points,
             Baseline baseline,
             Fill fill,
             Style style,
             SkColor color)
    : baseline_(baseline), fill_(fill), style_(style), color_(color) {
  DCHECK_LT(max_data_points, data_.BufferSize());
  max_data_points_ = std::min(max_data_points, data_.BufferSize() - 1);
}

Graph::~Graph() {}

void Graph::AddValue(float value, float unscaled_value) {
  data_.SaveToBuffer(value);
  unscaled_data_.SaveToBuffer(unscaled_value);
}

void Graph::Layout(const gfx::Rect& graph_bounds, const Graph* base) {
  graph_bounds_ = graph_bounds;

  const float scale_x = graph_bounds.width() / (float)max_data_points_;

  // Assume data is already scaled to [0-1], which will map to full
  // |graph_bounds| height.
  const float scale_y = graph_bounds.height();

  // Let every graph to occupy at least given amount of pixels.
  const int pixel_adjust = (baseline_ == Baseline::kBaselineBottom) ? -1 : 1;

  // Bottom path is always base. So it's visually below the current line when
  // kBaselineBottom and above current graph when BASELINE_TOP.
  // top_path_ is similarly inverted.
  bottom_path_.resize(0);
  if (base) {
    bottom_path_.reserve(base->top_path().size());
    bottom_path_style_ = base->style_;
    for (const SkPoint& point : base->top_path())
      bottom_path_.push_back({point.x(), point.y() + pixel_adjust});
  }
  top_path_.resize(0);
  top_path_.reserve(max_data_points_);
  size_t i = 0;
  // base::RingBuffer<> does not conform to C++ containers specification, so
  // it's End() is actually Back().
  for (Graph::Data::Iterator it = data_.End(); it; --it) {
    const float value = **it;

    float x = graph_bounds.x() + (max_data_points_ - i) * scale_x;
    float y =
        (baseline_ == Baseline::kBaselineBottom ? -1 : 1) * value * scale_y;
    if (bottom_path_.size()) {
      CHECK_LT(i, bottom_path_.size());
      // Adjust to the single pixel line added above.
      y = bottom_path_[i].y() - pixel_adjust + y;
    } else {
      y = (baseline_ == Baseline::kBaselineBottom ? graph_bounds.bottom()
                                                  : graph_bounds.y()) +
          y;
    }
    top_path_.push_back({x, y});
    ++i;
    if (i >= max_data_points_)
      break;
  }

  // This is the first layer from the start and it is filled and is non-empty.
  if (!base && fill_ != Graph::Fill::kNone && !top_path_.empty()) {
    gfx::RectF graph_bounds_f(graph_bounds);
    if (baseline_ == Baseline::kBaselineBottom) {
      bottom_path_.push_back({graph_bounds_f.right(), graph_bounds_f.bottom()});
      bottom_path_.push_back({top_path_.back().x(), graph_bounds_f.bottom()});
    } else {
      bottom_path_.push_back({graph_bounds_f.right(), graph_bounds_f.y()});
      bottom_path_.push_back({top_path_.back().x(), graph_bounds_f.y()});
    }
  }
}

void Graph::Draw(gfx::Canvas* canvas) const {
  if (top_path_.empty())
    return;

  SkPath path;
  path.moveTo(top_path_.front());

  const auto draw_top_line = [](const std::vector<SkPoint>& top_path,
                                const auto& draw_point, SkPath& out_path) {
    SkPoint previous_point = top_path.front();
    for (std::vector<SkPoint>::const_iterator it = top_path.begin();
         it != top_path.end(); ++it) {
      // For the top line we are already here.
      if (it == top_path.begin())
        continue;
      // Depending on the line type, |draw_point| may use the previous point.
      draw_point(*it, previous_point, out_path);
    }
  };
  const auto draw_bottom_line = [](const std::vector<SkPoint>& bottom_path,
                                   const auto& draw_point, SkPath& result) {
    SkPoint previous_point = bottom_path.back();
    for (std::vector<SkPoint>::const_reverse_iterator it =
             bottom_path.crbegin();
         it != bottom_path.crend(); ++it) {
      // Bottom line needs line to the first point too.
      // Depending on the line type, |draw_point| may use the previous point.
      draw_point(*it, previous_point, result);
    }
  };

  // This is used to draw both top and bottom Style::kLines paths.
  const auto draw_lines_point =
      [](const SkPoint& point, const SkPoint& /*previous_point*/,
         SkPath& out_path) { out_path.lineTo(point); };
  // Top and bottom Style::kSkyline drawing functions are symmetric.
  const auto draw_skyline_point =
      [](const SkPoint& point, SkPoint& previous_point, SkPath& out_path) {
        out_path.lineTo(SkPoint::Make(point.x(), previous_point.y()));
        out_path.lineTo(point);
        previous_point = point;
      };
  const auto draw_bottom_skyline_point =
      [](const SkPoint& point, SkPoint& previous_point, SkPath& out_path) {
        out_path.lineTo(SkPoint::Make(previous_point.x(), point.y()));
        out_path.lineTo(point);
        previous_point = point;
      };
  switch (style_) {
    case Style::kLines:
      draw_top_line(top_path_, draw_lines_point, path);
      break;
    case Style::kSkyline:
      draw_top_line(top_path_, draw_skyline_point, path);
      break;
  }
  if (fill_ == Graph::Fill::kSolid) {
    switch (bottom_path_style_) {
      case Style::kLines:
        draw_bottom_line(bottom_path_, draw_lines_point, path);
        break;
      case Style::kSkyline:
        draw_bottom_line(bottom_path_, draw_bottom_skyline_point, path);
        break;
    }
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  const cc::PaintFlags::Style style = (fill_ == Graph::Fill::kNone)
                                          ? cc::PaintFlags::kStroke_Style
                                          : cc::PaintFlags::kFill_Style;
  flags.setStyle(style);
  flags.setStrokeWidth(1);
  flags.setColor(color_);
  canvas->DrawPath(path, flags);
}

void Graph::UpdateLastValue(float value, float unscaled_value) {
  const size_t last_index = data_.BufferSize() - 1;
  if (!data_.IsFilledIndex(last_index))
    return;

  *data_.MutableReadBuffer(last_index) = value;
  *unscaled_data_.MutableReadBuffer(last_index) = unscaled_value;
}

float Graph::GetUnscaledValueAt(size_t index) const {
  if (index >= max_data_points_)
    return 0;

  // 0 - the oldest value
  // BufferSize() - 1  - the newest value.
  const size_t raw_index = index < unscaled_data_.BufferSize()
                               ? unscaled_data_.BufferSize() - 1 - index
                               : 0;

  // It will CHECK() if index is not populated.
  if (!unscaled_data_.IsFilledIndex(raw_index))
    return 0;

  return unscaled_data_.ReadBuffer(raw_index);
}

bool Graph::IsFilledIndex(size_t index) const {
  if (index >= max_data_points_)
    return false;

  // 0 - the oldest value
  // BufferSize() - 1  - the newest value.
  const size_t raw_index =
      index < data_.BufferSize() ? data_.BufferSize() - 1 - index : 0;
  return data_.IsFilledIndex(raw_index);
}

void Graph::Reset() {
  data_.Clear();
  unscaled_data_.Clear();
}

#if !defined(NDEBUG)
std::string Graph::DebugDump(const std::string& name) const {
  std::ostringstream os;
  os << name << ": location BLxy  [" << graph_bounds_.x() << ", "
     << graph_bounds_.bottom() << "] TRxy [" << graph_bounds_.right() << ", "
     << graph_bounds_.y() << "]";
  const int topsize = static_cast<int>(top_path_.size());
  for (int i = 0; i < topsize; ++i) {
    if ((i > 5) && (i < topsize - 5)) {
      // Skip the middle part.
      os << "\t" << name << ": ...";
      i = topsize - 5;
    }
    if (fill_ == Graph::Fill::kSolid) {
      // Print filled graph as a set of vertical lines.
      if (top_path_.size() == bottom_path_.size()) {
        // Each point on the top has matching point on the bottom.
        os << "\t" << name << ": " << i << ": [" << bottom_path_[i].x() << ", "
           << bottom_path_[i].y() << "] -> [" << top_path_[i].x() << ", "
           << top_path_[i].y() << "]";
      } else {
        // This is the first graph in stack. Use bottom_path_[0].y() as
        // reference.
        os << "\t" << name << ": " << i << ": ["
           << ((i == 0) ? bottom_path_[0].x() : top_path_[i].x()) << ", "
           << bottom_path_[0].y() << "] -> [" << top_path_[i].x() << ", "
           << top_path_[i].y() << "]";
      }
    } else {
      // Print lines graph as a list of dots.
      os << "\t" << name << ": " << i << ": -> [" << top_path_[i].x() << ", "
         << top_path_[i].y() << "]";
    }
    os << "\n";
  }
  return os.str();
}
#endif

}  // namespace hud_display
}  // namespace ash
