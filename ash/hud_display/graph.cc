// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/canvas.h"

namespace ash {
namespace hud_display {

Graph::Graph(Baseline baseline, Fill fill, SkColor color)
    : baseline_(baseline), fill_(fill), color_(color) {}

Graph::~Graph() {}

void Graph::AddValue(float value, float unscaled_value) {
  data_.SaveToBuffer(value);
  unscaled_data_.SaveToBuffer(unscaled_value);
}

void Graph::Layout(const gfx::Rect& graph_bounds, const Graph* base) {
  graph_bounds_ = graph_bounds;

  // base::RingBuffer::Iterator::index never reaches 0. Thus "-1";
  const float scale_x = graph_bounds.width() / (data_.BufferSize() - 1.f);

  // Assume data is already scaled to [0-1], which will map to full
  // |graph_bounds| height.
  const float scale_y = graph_bounds.height();

  // Let every graph to occupy at least given amount of pixels.
  const int pixel_adjust = (baseline_ == Baseline::BASELINE_BOTTOM) ? -1 : 1;

  // Bottom path is always base. So it's visually below the current line when
  // BASELINE_BOTTOM and above current graph when BASELINE_TOP.
  // top_path_ is similarly inverted.
  bottom_path_.resize(0);
  if (base) {
    bottom_path_.reserve(base->top_path().size());
    for (const SkPoint& point : base->top_path())
      bottom_path_.push_back({point.x(), point.y() + pixel_adjust});
  }
  top_path_.resize(0);
  top_path_.reserve(data_.BufferSize());
  size_t i = 0;
  // base::RingBuffer<> does not conform to C++ containers specification, so
  // it's End() is actually Back().
  for (Graph::Data::Iterator it = data_.End(); it; --it) {
    const float value = **it;

    float x = graph_bounds.x() + it.index() * scale_x;
    float y =
        (baseline_ == Baseline::BASELINE_BOTTOM ? -1 : 1) * value * scale_y;
    if (bottom_path_.size()) {
      CHECK_LT(i, bottom_path_.size());
      // Adjust to the single pixel line added above.
      y = bottom_path_[i].y() - pixel_adjust + y;
    } else {
      y = (baseline_ == Baseline::BASELINE_BOTTOM ? graph_bounds.bottom()
                                                  : graph_bounds.y()) +
          y;
    }
    top_path_.push_back({x, y});
    ++i;
  }

  // This is the first layer from the start and it is filled and is non-empty.
  if (!base && fill_ != Graph::Fill::NONE && !top_path_.empty()) {
    if (baseline_ == Baseline::BASELINE_BOTTOM) {
      bottom_path_.push_back({graph_bounds.right(), graph_bounds.bottom()});
      bottom_path_.push_back({top_path_.back().x(), graph_bounds.bottom()});
    } else {
      bottom_path_.push_back({graph_bounds.right(), graph_bounds.y()});
      bottom_path_.push_back({top_path_.back().x(), graph_bounds.y()});
    }
  }
}

void Graph::Draw(gfx::Canvas* canvas) const {
  if (top_path_.empty())
    return;

  SkPath path;
  path.moveTo(top_path_.front());
  CHECK(top_path_.size());
  for (std::vector<SkPoint>::const_iterator it = top_path_.begin();
       it != top_path_.end(); ++it) {
    if (it == top_path_.begin())
      continue;

    path.lineTo(*it);
  }
  if (fill_ == Graph::Fill::SOLID) {
    for (std::vector<SkPoint>::const_reverse_iterator it =
             bottom_path_.crbegin();
         it != bottom_path_.crend(); ++it) {
      path.lineTo(*it);
    }
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  const cc::PaintFlags::Style style = (fill_ == Graph::Fill::NONE)
                                          ? cc::PaintFlags::kStroke_Style
                                          : cc::PaintFlags::kFill_Style;
  flags.setStyle(style);
  flags.setStrokeWidth(1);
  flags.setColor(color_);
  canvas->DrawPath(path, flags);
}

float Graph::GetUnscaledValueAt(size_t index) const {
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
  // 0 - the oldest value
  // BufferSize() - 1  - the newest value.
  const size_t raw_index =
      index < data_.BufferSize() ? data_.BufferSize() - 1 - index : 0;
  return data_.IsFilledIndex(raw_index);
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
    if (fill_ == Graph::Fill::SOLID) {
      // Print filled graph as a set of vertical lines.
      if (top_path_.size() == bottom_path_.size()) {
        // Each point op the top has matching point on the bottom.
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
  }
  return os.str();
}
#endif

}  // namespace hud_display
}  // namespace ash
