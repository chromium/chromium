// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"

#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

ScrollTimeline::ScrollTimeline(base::Optional<ElementId> scroller_id,
                               ScrollDirection orientation,
                               base::Optional<double> start_scroll_offset,
                               base::Optional<double> end_scroll_offset,
                               double time_range)
    : active_id_(),
      pending_id_(scroller_id),
      orientation_(orientation),
      start_scroll_offset_(start_scroll_offset),
      end_scroll_offset_(end_scroll_offset),
      time_range_(time_range) {}

ScrollTimeline::~ScrollTimeline() {}

std::unique_ptr<ScrollTimeline> ScrollTimeline::CreateImplInstance() const {
  return std::make_unique<ScrollTimeline>(pending_id_, orientation_,
                                          start_scroll_offset_,
                                          end_scroll_offset_, time_range_);
}

double ScrollTimeline::CurrentTime(const ScrollTree& scroll_tree,
                                   bool is_active_tree) const {
  // We may be asked for the CurrentTime before the pending tree with our
  // scroller has been activated, or after the scroller has been removed (e.g.
  // if it is no longer composited). In these cases the best we can do is to
  // return an unresolved time value.
  if ((is_active_tree && !active_id_) || (!is_active_tree && !pending_id_))
    return std::numeric_limits<double>::quiet_NaN();

  ElementId scroller_id =
      is_active_tree ? active_id_.value() : pending_id_.value();

  // The scroller may not be in the ScrollTree if it is not currently scrollable
  // (e.g. has overflow: visible). By the spec, return an unresolved time value.
  const ScrollNode* scroll_node =
      scroll_tree.FindNodeFromElementId(scroller_id);
  if (!scroll_node)
    return std::numeric_limits<double>::quiet_NaN();

  gfx::ScrollOffset offset =
      scroll_tree.GetPixelSnappedScrollOffset(scroll_node->id);
  DCHECK_GE(offset.x(), 0);
  DCHECK_GE(offset.y(), 0);

  gfx::ScrollOffset scroll_dimensions =
      scroll_tree.MaxScrollOffset(scroll_node->id);

  double current_offset = (orientation_ == Vertical) ? offset.y() : offset.x();
  double max_offset = (orientation_ == Vertical) ? scroll_dimensions.y()
                                                 : scroll_dimensions.x();
  DCHECK_GE(current_offset, 0);
  DCHECK_GE(max_offset, 0);

  double resolved_start_scroll_offset = start_scroll_offset_.value_or(0);
  double resolved_end_scroll_offset = end_scroll_offset_.value_or(max_offset);

  // 3. If current scroll offset is less than startScrollOffset, return an
  // unresolved time value if fill is none or forwards, or 0 otherwise.
  // TODO(smcgruer): Implement |fill|.
  if (current_offset < resolved_start_scroll_offset) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  // 4. If current scroll offset is greater than or equal to endScrollOffset,
  // return an unresolved time value if fill is none or backwards, or the
  // effective time range otherwise.
  // TODO(smcgruer): Implement |fill|.
  //
  // Note we deliberately break the spec here by only returning if the current
  // offset is strictly greater, as that is more in line with the web animation
  // spec. See https://github.com/WICG/scroll-animations/issues/19
  if (current_offset > resolved_end_scroll_offset) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  // This is not by the spec, but avoids both negative current time and a
  // division by zero issue. See
  // https://github.com/WICG/scroll-animations/issues/20 and
  // https://github.com/WICG/scroll-animations/issues/21
  if (resolved_start_scroll_offset >= resolved_end_scroll_offset) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  // 5. Return the result of evaluating the following expression:
  //   ((current scroll offset - startScrollOffset) /
  //      (endScrollOffset - startScrollOffset)) * effective time range
  return ((current_offset - resolved_start_scroll_offset) /
          (resolved_end_scroll_offset - resolved_start_scroll_offset)) *
         time_range_;
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
