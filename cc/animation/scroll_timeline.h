// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_TIMELINE_H_
#define CC_ANIMATION_SCROLL_TIMELINE_H_

#include "base/optional.h"
#include "cc/animation/animation_export.h"
#include "cc/trees/element_id.h"

namespace cc {

class ScrollTree;

// A ScrollTimeline is an animation timeline that bases its current time on the
// progress of scrolling in some scroll container.
//
// This is the compositor-side representation of the web concept expressed in
// https://wicg.github.io/scroll-animations/#scrolltimeline-interface. There are
// differences between this class and the web definition of a ScrollTimeline.
// For example the compositor does not know (or care) about 'writing modes', so
// this class only tracks whether the ScrollTimeline orientation is horizontal
// or vertical. Blink is expected to resolve any such 'web' requirements and
// construct/update the compositor ScrollTimeline accordingly.
class CC_ANIMATION_EXPORT ScrollTimeline {
 public:
  enum ScrollDirection { Horizontal, Vertical };

  ScrollTimeline(base::Optional<ElementId> scroller_id,
                 ScrollDirection orientation,
                 base::Optional<double> start_scroll_offset,
                 base::Optional<double> end_scroll_offset,
                 double time_range);
  virtual ~ScrollTimeline();

  // Create a copy of this ScrollTimeline intended for the impl thread in the
  // compositor.
  std::unique_ptr<ScrollTimeline> CreateImplInstance() const;

  // Calculate the current time of the ScrollTimeline. This is either a double
  // value or std::numeric_limits<double>::quiet_NaN() if the current time is
  // unresolved.
  virtual double CurrentTime(const ScrollTree& scroll_tree,
                             bool is_active_tree) const;

  void SetScrollerId(base::Optional<ElementId> scroller_id);
  void UpdateStartAndEndScrollOffsets(
      base::Optional<double> start_scroll_offset,
      base::Optional<double> end_scroll_offset);

  void PushPropertiesTo(ScrollTimeline* impl_timeline);

  void PromoteScrollTimelinePendingToActive();

 private:
  // The scroller which this ScrollTimeline is based on. The same underlying
  // scroll source may have different ids in the pending and active tree (see
  // http://crbug.com/847588).
  base::Optional<ElementId> active_id_;
  base::Optional<ElementId> pending_id_;

  // The orientation of the ScrollTimeline indicates which axis of the scroller
  // it should base its current time on - either the horizontal or vertical.
  ScrollDirection orientation_;

  base::Optional<double> start_scroll_offset_;
  base::Optional<double> end_scroll_offset_;

  // A ScrollTimeline maps from the scroll offset in the scroller to a time
  // value based on a 'time range'. See the implementation of CurrentTime or the
  // spec for details.
  double time_range_;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_TIMELINE_H_
