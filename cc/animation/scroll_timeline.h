// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_TIMELINE_H_
#define CC_ANIMATION_SCROLL_TIMELINE_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"

namespace cc {

class ScrollTree;

// A ScrollTimeline is an animation timeline that bases its current time on the
// progress of scrolling in some scroll container.
//
// This is the compositor-side representation of the web concept expressed in
// https://wicg.github.io/scroll-animations/#scrolltimeline-interface.
class CC_ANIMATION_EXPORT ScrollTimeline : public AnimationTimeline {
 public:
  // cc does not know about writing modes. The ScrollDirection below is
  // converted using blink::scroll_timeline_util::ConvertOrientation which takes
  // the spec-compliant ScrollDirection enumeration.
  // https://drafts.csswg.org/scroll-animations/#scrolldirection-enumeration
  enum ScrollDirection {
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight,
  };

  struct ScrollOffsets {
    ScrollOffsets() = default;
    ScrollOffsets(double start_offset, double end_offset) {
      start = start_offset;
      end = end_offset;
    }
    bool operator==(const ScrollOffsets& other) const {
      return start == other.start && end == other.end;
    }
    bool operator!=(const ScrollOffsets& other) const {
      return !(*this == other);
    }

    double start = 0;
    double end = 0;
  };

  // Fixed time scale converting from pixels to microseconds.
  // The value is derived from error analysis of the quantization of pixels in
  // LayoutUnits.  The quantization is 1/64 of a pixel, and maximum possible
  // error in current time calculations is 4 times that amount as shown below.
  //
  // progress = (scroll - start) / (end - start)
  // Positions are subject to imprecision based on quantization.
  // For worst case analysis, we compute the difference between the maximum
  // and minimum progress based on the allowable error:
  // progress = ((current offset +/- delta) - (start +/- delta) /
  //            ((end +/- delta) - (start +/- delta))
  // where delta = kLengthPrecision = 1 / kFixedPointDenominator = 1 / 64
  //
  // To minimum, we take the smallest possible numerator and largest possible
  // denominator, which means minimizing current offset and maximizing cover
  // end time.  The cover start time appears in both the numerator and
  // denominator, but has a large impact on the numerator. Thus,
  //
  // min = ((current offset - delta) - (start + delta)) /
  //       ((end + delta) - (start + delta))
  //     = (current offset - start - 2 * delta) / range
  // max = ((current offset + delta) - (start + delta)) /
  //       ((end - delta) - (start + delta))
  //     = (current offset - start + 2 * delta) / range;
  // max error = max - min = 4 * delta / range
  // duration = 1 [microsecond] / error
  //          = range / (4 / 64)
  //          = 16 range
  static constexpr double kScrollTimelineMicrosecondsPerPixel = 16;

  ScrollTimeline(std::optional<ElementId> scroller_id,
                 ScrollDirection direction,
                 std::optional<ScrollOffsets> scroll_offsets,
                 int animation_timeline_id);

  static scoped_refptr<ScrollTimeline> Create(
      std::optional<ElementId> scroller_id,
      ScrollDirection direction,
      std::optional<ScrollOffsets> scroll_offsets);

  // Create a copy of this ScrollTimeline intended for the impl thread in the
  // compositor.
  scoped_refptr<AnimationTimeline> CreateImplInstance() const override;

  // ScrollTimeline is active if the scroll node exists in active or pending
  // scroll tree.
  virtual bool IsActive(const ScrollTree& scroll_tree,
                        bool is_active_tree) const;

  // Calculate the current time of the ScrollTimeline. This is either a
  // base::TimeTicks value or std::nullopt if the current time is unresolved.
  // The internal calculations are performed using doubles and the result is
  // converted to base::TimeTicks. This limits the precision to 1us.
  virtual std::optional<base::TimeTicks> CurrentTime(
      const ScrollTree& scroll_tree,
      bool is_active_tree) const;

  virtual std::optional<base::TimeTicks> Duration(const ScrollTree& scroll_tree,
                                                  bool is_active_tree) const;

  void UpdateScrollerIdAndScrollOffsets(
      std::optional<ElementId> scroller_id,
      std::optional<ScrollOffsets> scroll_offsets);

  void PushPropertiesTo(AnimationTimeline* impl_timeline) override;
  void ActivateTimeline() override;

  bool TickScrollLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      const ScrollTree& scroll_tree,
      bool is_active_tree) override;

  std::optional<ElementId> GetActiveIdForTest() const { return active_id(); }
  std::optional<ElementId> GetPendingIdForTest() const { return pending_id(); }
  ScrollDirection GetDirectionForTest() const { return direction(); }
  std::optional<double> GetStartScrollOffsetForTest() const {
    std::optional<ScrollOffsets> offsets = pending_offsets();
    if (offsets) {
      return offsets->start;
    }
    return std::nullopt;
  }
  std::optional<double> GetEndScrollOffsetForTest() const {
    std::optional<ScrollOffsets> offsets = pending_offsets();
    if (offsets) {
      return offsets->end;
    }
    return std::nullopt;
  }

  bool IsScrollTimeline() const override;
  bool IsLinkedToScroller(ElementId scroller) const override;

 protected:
  ~ScrollTimeline() override;

 private:
  const std::optional<ElementId>& active_id() const {
    return active_id_.Read(*this);
  }

  const std::optional<ElementId>& pending_id() const {
    return pending_id_.Read(*this);
  }

  const ScrollDirection& direction() const { return direction_.Read(*this); }

  const std::optional<ScrollOffsets>& active_offsets() const {
    return active_offsets_.Read(*this);
  }

  const std::optional<ScrollOffsets>& pending_offsets() const {
    return pending_offsets_.Read(*this);
  }

  // The scroller which this ScrollTimeline is based on. The same underlying
  // scroll source may have different ids in the pending and active tree (see
  // http://crbug.com/847588).

  // Only the impl thread can set active properties.
  ProtectedSequenceForbidden<std::optional<ElementId>> active_id_;
  ProtectedSequenceWritable<std::optional<ElementId>> pending_id_;

  // The direction of the ScrollTimeline indicates which axis of the scroller
  // it should base its current time on, and where the origin point is.
  ProtectedSequenceReadable<ScrollDirection> direction_;

  ProtectedSequenceForbidden<std::optional<ScrollOffsets>> active_offsets_;
  ProtectedSequenceWritable<std::optional<ScrollOffsets>> pending_offsets_;
};

inline ScrollTimeline* ToScrollTimeline(AnimationTimeline* timeline) {
  DCHECK(timeline->IsScrollTimeline());
  return static_cast<ScrollTimeline*>(timeline);
}

inline const ScrollTimeline* ToScrollTimeline(
    const AnimationTimeline* timeline) {
  DCHECK(timeline->IsScrollTimeline());
  return static_cast<const ScrollTimeline*>(timeline);
}

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_TIMELINE_H_
