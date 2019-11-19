// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"

#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"

#include <memory>

namespace cc {

namespace {
bool IsVertical(ScrollTimeline::ScrollDirection direction) {
  return direction == ScrollTimeline::ScrollUp ||
         direction == ScrollTimeline::ScrollDown;
}

bool IsReverse(ScrollTimeline::ScrollDirection direction) {
  return direction == ScrollTimeline::ScrollUp ||
         direction == ScrollTimeline::ScrollLeft;
}
}  // namespace

ScrollTimeline::ScrollTimeline(base::Optional<ElementId> scroller_id,
                               ScrollDirection direction,
                               base::Optional<double> start_scroll_offset,
                               base::Optional<double> end_scroll_offset,
                               double time_range,
                               KeyframeModel::FillMode fill)
    : active_id_(),
      pending_id_(scroller_id),
      direction_(direction),
      start_scroll_offset_(start_scroll_offset),
      end_scroll_offset_(end_scroll_offset),
      time_range_(time_range),
      fill_(fill) {}

ScrollTimeline::~ScrollTimeline() {}

std::unique_ptr<ScrollTimeline> ScrollTimeline::CreateImplInstance() const {
  return std::make_unique<ScrollTimeline>(
      pending_id_, direction_, start_scroll_offset_, end_scroll_offset_,
      time_range_, fill_);
}

bool ScrollTimeline::IsActive(const ScrollTree& scroll_tree,
                              bool is_active_tree) const {
  // If pending tree with our scroller hasn't been activated, or the scroller
  // has been removed (e.g. if it is no longer composited).
  if ((is_active_tree && !active_id_) || (!is_active_tree && !pending_id_))
    return false;

  ElementId scroller_id =
      is_active_tree ? active_id_.value() : pending_id_.value();
  // The scroller is not in the ScrollTree if it is not currently scrollable
  // (e.g. has overflow: visible). In this case the timeline is not active.
  return scroll_tree.FindNodeFromElementId(scroller_id);
}

base::Optional<base::TimeTicks> ScrollTimeline::CurrentTime(
    const ScrollTree& scroll_tree,
    bool is_active_tree) const {
  // If the timeline is not active return unresolved value by the spec.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  if (!IsActive(scroll_tree, is_active_tree))
    return base::nullopt;

  ElementId scroller_id =
      is_active_tree ? active_id_.value() : pending_id_.value();
  const ScrollNode* scroll_node =
      scroll_tree.FindNodeFromElementId(scroller_id);

  gfx::ScrollOffset offset =
      scroll_tree.GetPixelSnappedScrollOffset(scroll_node->id);
  DCHECK_GE(offset.x(), 0);
  DCHECK_GE(offset.y(), 0);

  gfx::ScrollOffset scroll_dimensions =
      scroll_tree.MaxScrollOffset(scroll_node->id);

  double max_offset =
      IsVertical(direction_) ? scroll_dimensions.y() : scroll_dimensions.x();
  double current_physical_offset =
      IsVertical(direction_) ? offset.y() : offset.x();
  double current_offset = IsReverse(direction_)
                              ? max_offset - current_physical_offset
                              : current_physical_offset;
  DCHECK_GE(max_offset, 0);
  DCHECK_GE(current_offset, 0);

  double resolved_start_scroll_offset = start_scroll_offset_.value_or(0);
  double resolved_end_scroll_offset = end_scroll_offset_.value_or(max_offset);

  // 3. If current scroll offset is less than startScrollOffset:
  if (current_offset < resolved_start_scroll_offset) {
    // Return an unresolved time value if fill is none or forwards.
    if (fill_ == KeyframeModel::FillMode::NONE ||
        fill_ == KeyframeModel::FillMode::FORWARDS) {
      return base::nullopt;
    }

    // Otherwise, return 0.
    return base::TimeTicks();
  }

  // 4. If current scroll offset is greater than or equal to endScrollOffset:
  if (current_offset >= resolved_end_scroll_offset) {
    // If endScrollOffset is less than the maximum scroll offset of scrollSource
    // in orientation and fill is none or backwards, return an unresolved time
    // value.
    if (resolved_end_scroll_offset < max_offset &&
        (fill_ == KeyframeModel::FillMode::NONE ||
         fill_ == KeyframeModel::FillMode::BACKWARDS)) {
      return base::nullopt;
    }

    // Otherwise, return the effective time range.
    return base::TimeTicks() + base::TimeDelta::FromMillisecondsD(time_range_);
  }

  // This is not by the spec, but avoids a negative current time.
  // See https://github.com/WICG/scroll-animations/issues/20
  if (resolved_start_scroll_offset >= resolved_end_scroll_offset) {
    return base::nullopt;
  }

  // 5. Return the result of evaluating the following expression:
  //   ((current scroll offset - startScrollOffset) /
  //      (endScrollOffset - startScrollOffset)) * effective time range
  return base::TimeTicks() +
         base::TimeDelta::FromMillisecondsD(
             ((current_offset - resolved_start_scroll_offset) /
              (resolved_end_scroll_offset - resolved_start_scroll_offset)) *
             time_range_);
}

void ScrollTimeline::PushPropertiesTo(ScrollTimeline* impl_timeline) {
  DCHECK(impl_timeline);
  impl_timeline->pending_id_ = pending_id_;
  // TODO(smcgruer): This leads to incorrect behavior in the current design,
  // because we end up using the pending start/end scroll offset for the active
  // tree too. Instead we need to either split these (like pending_id_ and
  // active_id_) or have a ScrollTimeline per tree.
  impl_timeline->start_scroll_offset_ = start_scroll_offset_;
  impl_timeline->end_scroll_offset_ = end_scroll_offset_;
}

void ScrollTimeline::PromoteScrollTimelinePendingToActive() {
  active_id_ = pending_id_;
}

void ScrollTimeline::UpdateStartAndEndScrollOffsets(
    base::Optional<double> start_scroll_offset,
    base::Optional<double> end_scroll_offset) {
  start_scroll_offset_ = start_scroll_offset;
  end_scroll_offset_ = end_scroll_offset;
}

void ScrollTimeline::SetScrollerId(base::Optional<ElementId> pending_id) {
  // When the scroller id changes it will first be modified in the pending tree.
  // Then later (when the pending tree is promoted to active)
  // |PromoteScrollTimelinePendingToActive| will be called and will set the
  // |active_id_|.
  pending_id_ = pending_id;
}

}  // namespace cc
