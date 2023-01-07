// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_focus_ring_group.h"

#include <memory>
#include <set>
#include <vector>

#include "ash/accessibility/ui/accessibility_focus_ring.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_layer.h"
#include "ash/accessibility/ui/layer_animation_info.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "base/memory/values_equivalent.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

// The number of DIPs the focus ring is outset from the object it outlines,
// which also determines the border radius of the rounded corners.
constexpr int kAccessibilityFocusRingMargin = 7;

// Time to transition between one location and the next.
constexpr int kTransitionTimeMilliseconds = 300;

// Focus constants.
constexpr int kFocusFadeInTimeMilliseconds = 100;
constexpr int kFocusFadeOutTimeMilliseconds = 1600;

// A Region is an unordered collection of Rects that maintains its
// bounding box. Used in the middle of an algorithm that groups
// adjacent and overlapping rects.
struct Region {
  explicit Region(gfx::Rect initial_rect) {
    bounds = initial_rect;
    rects.push_back(initial_rect);
  }
  gfx::Rect bounds;
  std::vector<gfx::Rect> rects;
};

}  // namespace

AccessibilityFocusRingGroup::AccessibilityFocusRingGroup() {
  focus_animation_info_.fade_in_time =
      base::Milliseconds(kFocusFadeInTimeMilliseconds);
  focus_animation_info_.fade_out_time =
      base::Milliseconds(kFocusFadeOutTimeMilliseconds);
}

AccessibilityFocusRingGroup::~AccessibilityFocusRingGroup() {}

void AccessibilityFocusRingGroup::UpdateFocusRingsFromInfo(
    AccessibilityLayerDelegate* delegate) {
  previous_focus_rings_.swap(focus_rings_);
  focus_rings_.clear();
  focus_animation_.reset();
  RectsToRings(focus_ring_info_->rects_in_screen, &(focus_rings_));
  focus_layers_.resize(focus_rings_.size());
  if (focus_rings_.empty())
    return;

  for (size_t i = 0; i < focus_rings_.size(); ++i) {
    if (!focus_layers_[i])
      focus_layers_[i] =
          std::make_unique<AccessibilityFocusRingLayer>(delegate);
  }

  if (focus_ring_info_->behavior == FocusRingBehavior::PERSIST &&
      !no_fade_for_testing_) {
    // In PERSIST mode, animate the first ring to its destination
    // location, then set the rest of the rings directly.
    // If no_fade_for_testing_ is set, don't wait for animation.
    for (size_t i = 1; i < focus_rings_.size(); ++i)
      focus_layers_[i]->Set(focus_rings_[i]);
  } else {
    // In FADE mode, set all focus rings to their destination location.
    for (size_t i = 0; i < focus_rings_.size(); ++i)
      focus_layers_[i]->Set(focus_rings_[i]);
  }

  for (size_t i = 0; i < focus_rings_.size(); ++i) {
    focus_layers_[i]->SetAppearance(
        focus_ring_info_->type, focus_ring_info_->stacking_order,
        focus_ring_info_->color, focus_ring_info_->secondary_color,
        focus_ring_info_->background_color);
  }

  // Start watching for animations.
  if (!no_fade_for_testing_) {
    focus_animation_ = std::make_unique<AccessibilityAnimationOneShot>(
        focus_rings_[0].GetBounds(),
        base::BindRepeating(&AccessibilityFocusRingGroup::AnimateFocusRings,
                            base::Unretained(this)));
  }
}

bool AccessibilityFocusRingGroup::AnimateFocusRings(base::TimeTicks timestamp) {
  CHECK(!focus_rings_.empty());
  CHECK(!focus_layers_.empty());
  CHECK(focus_layers_[0]);

  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // focus change, so we just treat those as a delta of zero.
  if (timestamp < focus_animation_info_.change_time)
    timestamp = focus_animation_info_.change_time;

  if (focus_ring_info_->behavior == FocusRingBehavior::PERSIST) {
    base::TimeDelta delta = timestamp - focus_animation_info_.change_time;
    base::TimeDelta transition_time =
        base::Milliseconds(kTransitionTimeMilliseconds);
    if (delta >= transition_time) {
      focus_layers_[0]->Set(focus_rings_[0]);
      return true;
    }

    // Ease-in effect.
    const double fraction = pow(delta / transition_time, 0.3);

    // Handle corner case where we're animating but we don't have previous
    // rings.
    if (previous_focus_rings_.empty())
      previous_focus_rings_ = focus_rings_;

    focus_layers_[0]->Set(AccessibilityFocusRing::Interpolate(
        previous_focus_rings_[0], focus_rings_[0], fraction));
  } else {
    ash::ComputeOpacity(&(focus_animation_info_), timestamp);
    for (auto& focus_layer : focus_layers_)
      focus_layer->SetOpacity(focus_animation_info_.opacity);
  }

  return false;
}

bool AccessibilityFocusRingGroup::UpdateFocusRing(
    std::unique_ptr<AccessibilityFocusRingInfo> focus_ring,
    AccessibilityLayerDelegate* delegate) {
  // Remove duplicate rects.
  if (focus_ring && focus_ring->rects_in_screen.size() > 1) {
    std::vector<gfx::Rect> rects(focus_ring->rects_in_screen);
    std::set<gfx::Rect> rects_set(rects.begin(), rects.end());
    focus_ring->rects_in_screen.assign(rects_set.begin(), rects_set.end());
  }

  // If there is no change, don't do any work.
  if (base::ValuesEquivalent(focus_ring_info_, focus_ring))
    return false;

  focus_ring_info_ = std::move(focus_ring);
  UpdateFocusRingsFromInfo(delegate);
  return true;
}

void AccessibilityFocusRingGroup::ClearFocusRects(
    AccessibilityLayerDelegate* delegate) {
  focus_ring_info_->rects_in_screen.clear();
  UpdateFocusRingsFromInfo(delegate);
}

int AccessibilityFocusRingGroup::GetMargin() const {
  return kAccessibilityFocusRingMargin;
}

void AccessibilityFocusRingGroup::RectsToRings(
    const std::vector<gfx::Rect>& src_rects,
    std::vector<ash::AccessibilityFocusRing>* rings) const {
  if (src_rects.empty())
    return;

  // Give all of the rects a margin.
  std::vector<gfx::Rect> rects;
  rects.resize(src_rects.size());
  for (size_t i = 0; i < src_rects.size(); ++i) {
    rects[i] = src_rects[i];
    rects[i].Inset(-GetMargin());
  }

  // Split the rects into contiguous regions.
  std::vector<Region> regions;
  regions.push_back(Region(rects[0]));
  for (size_t i = 1; i < rects.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < regions.size(); ++j) {
      if (Intersects(rects[i], regions[j].bounds)) {
        regions[j].rects.push_back(rects[i]);
        regions[j].bounds.Union(rects[i]);
        found = true;
      }
    }
    if (!found) {
      regions.push_back(Region(rects[i]));
    }
  }

  // Keep merging regions that intersect.
  // TODO(dmazzoni): reduce the worst-case complexity! This appears like
  // it could be O(n^3), make sure it's not in practice.
  bool merged;
  do {
    merged = false;
    for (size_t i = 0; i < regions.size() - 1 && !merged; ++i) {
      for (size_t j = i + 1; j < regions.size() && !merged; ++j) {
        if (Intersects(regions[i].bounds, regions[j].bounds)) {
          regions[i].rects.insert(regions[i].rects.end(),
                                  regions[j].rects.begin(),
                                  regions[j].rects.end());
          regions[i].bounds.Union(regions[j].bounds);
          regions.erase(regions.begin() + j);
          merged = true;
        }
      }
    }
  } while (merged);

  for (size_t i = 0; i < regions.size(); ++i) {
    std::sort(regions[i].rects.begin(), regions[i].rects.end());
    rings->push_back(RingFromSortedRects(regions[i].rects));
  }
}

// Given a vector of rects that all overlap, already sorted from top to bottom
// and left to right, split them into three shapes covering the top, middle,
// and bottom of a "paragraph shape".
//
// Input:
//
//                       +---+---+
//                       | 1 | 2 |
// +---------------------+---+---+
// |             3               |
// +--------+---------------+----+
// |    4   |         5     |
// +--------+---------------+--+
// |             6             |
// +---------+-----------------+
// |    7    |
// +---------+
//
// Output:
//
//                       +-------+
//                       |  Top  |
// +---------------------+-------+
// |                             |
// |                             |
// |           Middle            |
// |                             |
// |                             |
// +---------+-------------------+
// | Bottom  |
// +---------+
//
// When there's no clear "top" or "bottom" segment, split the overall rect
// evenly so that some of the area still fits into the "top" and "bottom"
// segments.
void AccessibilityFocusRingGroup::SplitIntoParagraphShape(
    const std::vector<gfx::Rect>& rects,
    gfx::Rect* top,
    gfx::Rect* middle,
    gfx::Rect* bottom) const {
  size_t n = rects.size();

  // Figure out how many rects belong in the top portion.
  gfx::Rect top_rect = rects[0];
  int top_middle = (top_rect.y() + top_rect.bottom()) / 2;
  size_t top_count = 1;
  while (top_count < n && rects[top_count].y() < top_middle) {
    top_rect.Union(rects[top_count]);
    top_middle = (top_rect.y() + top_rect.bottom()) / 2;
    top_count++;
  }

  // Figure out how many rects belong in the bottom portion.
  gfx::Rect bottom_rect = rects[n - 1];
  int bottom_middle = (bottom_rect.y() + bottom_rect.bottom()) / 2;
  size_t bottom_count = std::min(static_cast<size_t>(1), n - top_count);
  while (bottom_count + top_count < n &&
         rects[n - bottom_count - 1].bottom() > bottom_middle) {
    bottom_rect.Union(rects[n - bottom_count - 1]);
    bottom_middle = (bottom_rect.y() + bottom_rect.bottom()) / 2;
    bottom_count++;
  }

  // Whatever's left goes to the middle rect, but if there's no middle or
  // bottom rect, split the existing rects evenly to make one.
  gfx::Rect middle_rect;
  if (top_count + bottom_count < n) {
    middle_rect = rects[top_count];
    for (size_t i = top_count + 1; i < n - bottom_count; i++)
      middle_rect.Union(rects[i]);
  } else if (bottom_count > 0) {
    gfx::Rect enclosing_rect = top_rect;
    enclosing_rect.Union(bottom_rect);
    int middle_top = (top_rect.y() + top_rect.bottom() * 2) / 3;
    int middle_bottom = (bottom_rect.y() * 2 + bottom_rect.bottom()) / 3;
    top_rect.set_height(middle_top - top_rect.y());
    bottom_rect.set_height(bottom_rect.bottom() - middle_bottom);
    bottom_rect.set_y(middle_bottom);
    middle_rect = gfx::Rect(enclosing_rect.x(), middle_top,
                            enclosing_rect.width(), middle_bottom - middle_top);
  } else {
    int middle_top = (top_rect.y() * 2 + top_rect.bottom()) / 3;
    int middle_bottom = (top_rect.y() + top_rect.bottom() * 2) / 3;
    middle_rect = gfx::Rect(top_rect.x(), middle_top, top_rect.width(),
                            middle_bottom - middle_top);
    bottom_rect = gfx::Rect(top_rect.x(), middle_bottom, top_rect.width(),
                            top_rect.bottom() - middle_bottom);
    top_rect.set_height(middle_top - top_rect.y());
  }

  if (middle_rect.y() > top_rect.bottom()) {
    middle_rect.set_height(middle_rect.height() + middle_rect.y() -
                           top_rect.bottom());
    middle_rect.set_y(top_rect.bottom());
  }

  if (middle_rect.bottom() < bottom_rect.y()) {
    middle_rect.set_height(bottom_rect.y() - middle_rect.y());
  }

  *top = top_rect;
  *middle = middle_rect;
  *bottom = bottom_rect;
}

AccessibilityFocusRing AccessibilityFocusRingGroup::RingFromSortedRects(
    const std::vector<gfx::Rect>& rects) const {
  if (rects.size() == 1)
    return AccessibilityFocusRing::CreateWithRect(rects[0], GetMargin());

  gfx::Rect top;
  gfx::Rect middle;
  gfx::Rect bottom;
  SplitIntoParagraphShape(rects, &top, &middle, &bottom);

  return AccessibilityFocusRing::CreateWithParagraphShape(top, middle, bottom,
                                                          GetMargin());
}

bool AccessibilityFocusRingGroup::Intersects(const gfx::Rect& r1,
                                             const gfx::Rect& r2) const {
  int slop = GetMargin();
  return (r2.x() <= r1.right() + slop && r2.right() >= r1.x() - slop &&
          r2.y() <= r1.bottom() + slop && r2.bottom() >= r1.y() - slop);
}

}  // namespace ash
