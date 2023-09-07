// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_SNAP_DATA_H_
#define CC_INPUT_SCROLL_SNAP_DATA_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/range/range_f.h"

namespace cc {

class SnapSelectionStrategy;

// See https://www.w3.org/TR/css-scroll-snap-1/#snap-axis
enum class SnapAxis : unsigned {
  kBoth,
  kX,
  kY,
  kBlock,
  kInline,
};

// A helper enum to specify the the axis when doing calculations.
enum class SearchAxis : unsigned { kX, kY };

// See https://www.w3.org/TR/css-scroll-snap-1/#snap-strictness
enum class SnapStrictness : unsigned { kProximity, kMandatory };

// See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-align
enum class SnapAlignment : unsigned { kNone, kStart, kEnd, kCenter };

struct ScrollSnapType {
  ScrollSnapType()
      : is_none(true),
        axis(SnapAxis::kBoth),
        strictness(SnapStrictness::kProximity) {}

  ScrollSnapType(bool snap_type_none, SnapAxis axis, SnapStrictness strictness)
      : is_none(snap_type_none), axis(axis), strictness(strictness) {}

  bool operator==(const ScrollSnapType& other) const {
    return is_none == other.is_none && axis == other.axis &&
           strictness == other.strictness;
  }

  bool operator!=(const ScrollSnapType& other) const {
    return !(*this == other);
  }

  // Represents whether the scroll-snap-type is none.
  bool is_none;

  SnapAxis axis;
  SnapStrictness strictness;
};

struct ScrollSnapAlign {
  ScrollSnapAlign()
      : alignment_block(SnapAlignment::kNone),
        alignment_inline(SnapAlignment::kNone) {}

  explicit ScrollSnapAlign(SnapAlignment alignment)
      : alignment_block(alignment), alignment_inline(alignment) {}

  ScrollSnapAlign(SnapAlignment b, SnapAlignment i)
      : alignment_block(b), alignment_inline(i) {}

  bool operator==(const ScrollSnapAlign& other) const {
    return alignment_block == other.alignment_block &&
           alignment_inline == other.alignment_inline;
  }

  bool operator!=(const ScrollSnapAlign& other) const {
    return !(*this == other);
  }

  SnapAlignment alignment_block;
  SnapAlignment alignment_inline;
};

// This class includes snap offset and visible range needed to perform a snap
// operation on one axis for a specific area. The data can be used to determine
// whether this snap area provides a valid snap position for the current scroll.
class SnapSearchResult {
 public:
  SnapSearchResult() {}
  SnapSearchResult(float offset, const gfx::RangeF& range);
  // Clips the |snap_offset| between 0 and |max_snap|. And clips the
  // |visible_range| between 0 and |max_visible|.
  void Clip(float max_snap, float max_visible);

  // Union the visible_range of the two SnapSearchResult if they represent two
  // snap areas that are both covering the snapport at the current offset.
  // The |element_id_| of this is arbitrarily chosen because both snap areas
  // cover the snapport and are therefore both valid.
  void Union(const SnapSearchResult& other);

  float snap_offset() const { return snap_offset_; }
  void set_snap_offset(float offset) { snap_offset_ = offset; }

  gfx::RangeF visible_range() const { return visible_range_; }
  void set_visible_range(const gfx::RangeF& range);

  ElementId element_id() const { return element_id_; }
  void set_element_id(ElementId id) { element_id_ = id; }

  absl::optional<gfx::RangeF> covered_range() const { return covered_range_; }
  void set_covered_range(const gfx::RangeF& range) { covered_range_ = range; }

 private:
  // Scroll offset corresponding to this snap position. If covered_range_ is set
  // then this will be a position inside the range. In the covered case, the
  // result from FindClosestValidArea has a snap_offset_ equal to the
  // intended_position() of the SnapSelectionStrategy.
  // TODO(crbug.com/1472410): With refactoring it may be possible to replace
  // snap_offset_ and covered_range_ with a single range field with start == end
  // for "aligned" snap positions.
  float snap_offset_;
  // This is the range on the cross axis, within which the SnapArea generating
  // this |snap_offset| is visible. We expect the range to be in order (as
  // opposed to reversed), i.e., start() < end().
  gfx::RangeF visible_range_;

  // The ElementId of the snap area that corresponds to this SnapSearchResult.
  ElementId element_id_;

  // This is set if the validity of this result derives from the fact that the
  // snap area covers the viewport, as described in the spec section on
  // "Snapping Boxes that Overflow the Scrollport":
  // https://drafts.csswg.org/css-scroll-snap-1/#snap-overflow
  //
  // If set, indicates the range of scroll offsets for which the snap area
  // covers the viewport. The snap_offset_ will be a point within this range.
  absl::optional<gfx::RangeF> covered_range_;
};

// Snap area is a bounding box that could be snapped to when a scroll happens in
// its scroll container.
// This data structure describes the data needed for SnapCoordinator if we want
// to snap to this snap area.
struct SnapAreaData {
  // kInvalidScrollOffset is used to mark that the snap_position on a specific
  // axis is not applicable, thus should not be considered when snapping on that
  // axis. This is because the snap area has SnapAlignmentNone on that axis.
  static const int kInvalidScrollPosition = -1;

  SnapAreaData() {}

  SnapAreaData(const ScrollSnapAlign& align,
               const gfx::RectF& rec,
               bool msnap,
               ElementId id)
      : scroll_snap_align(align), rect(rec), must_snap(msnap), element_id(id) {}

  bool operator==(const SnapAreaData& other) const {
    return (other.element_id == element_id) &&
           (other.scroll_snap_align == scroll_snap_align) &&
           (other.rect == rect) && (other.must_snap == must_snap);
  }

  bool operator!=(const SnapAreaData& other) const { return !(*this == other); }

  // Specifies how the snap area should be aligned with its snap container when
  // snapped. The alignment_inline and alignment_block represent the alignments
  // on x axis and y axis repectively.
  ScrollSnapAlign scroll_snap_align;

  // The snap area rect relative to its snap container's boundary
  gfx::RectF rect;

  // Whether this area has scroll-snap-stop: always.
  // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-stop
  bool must_snap;

  // ElementId of the corresponding snap area.
  ElementId element_id;
};

struct TargetSnapAreaElementIds {
  TargetSnapAreaElementIds() = default;
  TargetSnapAreaElementIds(ElementId x_id, ElementId y_id) : x(x_id), y(y_id) {}
  bool operator==(const TargetSnapAreaElementIds& other) const {
    return (other.x == x) && (other.y == y);
  }

  bool operator!=(const TargetSnapAreaElementIds& other) const {
    return !(*this == other);
  }

  // Note that the same element can be snapped to on both the x and y axes.
  ElementId x;
  ElementId y;
};

typedef std::vector<SnapAreaData> SnapAreaList;

// Represents the result of a call to SnapContainerData::FindSnapPosition.
struct SnapPositionData {
  enum class Type { kNone, kAligned, kCovered };

  // What kind of snap position (if any) was found.
  Type type = Type::kNone;

  // The scroll offset of the snap position.
  gfx::PointF position;

  // The elements generating the snap areas on both axes.
  TargetSnapAreaElementIds target_element_ids;

  absl::optional<gfx::RangeF> covered_range_x;
  absl::optional<gfx::RangeF> covered_range_y;
};

// Snap container is a scroll container that at least one snap area assigned to
// it.  If the snap-type is not 'none', then it can be snapped to one of its
// snap areas when a scroll happens.
// This data structure describes the data needed for SnapCoordinator to perform
// snapping in the snap container.
//
// Note that the snap area data should only be used when snap-type is not 'none'
// There is not guarantee that this information is up-to-date otherwise. In
// fact, we skip updating these info as an optiomization.
class CC_EXPORT SnapContainerData {
 public:
  SnapContainerData();
  explicit SnapContainerData(ScrollSnapType type);
  SnapContainerData(ScrollSnapType type,
                    const gfx::RectF& rect,
                    const gfx::PointF& max);
  SnapContainerData(const SnapContainerData& other);
  SnapContainerData(SnapContainerData&& other);
  ~SnapContainerData();

  SnapContainerData& operator=(const SnapContainerData& other);
  SnapContainerData& operator=(SnapContainerData&& other);

  bool operator==(const SnapContainerData& other) const {
    return (other.scroll_snap_type_ == scroll_snap_type_) &&
           (other.rect_ == rect_) && (other.max_position_ == max_position_) &&
           (other.proximity_range_ == proximity_range_) &&
           (other.snap_area_list_ == snap_area_list_) &&
           (other.target_snap_area_element_ids_ ==
            target_snap_area_element_ids_);
  }

  bool operator!=(const SnapContainerData& other) const {
    return !(*this == other);
  }

  SnapPositionData FindSnapPosition(
      const SnapSelectionStrategy& strategy,
      const ElementId& active_element_id = ElementId()) const;

  const TargetSnapAreaElementIds& GetTargetSnapAreaElementIds() const;
  // Returns true if the target snap area element ids were changed.
  bool SetTargetSnapAreaElementIds(TargetSnapAreaElementIds ids);

  void AddSnapAreaData(SnapAreaData snap_area_data);
  size_t size() const { return snap_area_list_.size(); }
  const SnapAreaData& at(size_t index) const { return snap_area_list_[index]; }

  void set_scroll_snap_type(ScrollSnapType type) { scroll_snap_type_ = type; }
  ScrollSnapType scroll_snap_type() const { return scroll_snap_type_; }

  void set_rect(const gfx::RectF& rect) { rect_ = rect; }
  gfx::RectF rect() const { return rect_; }

  void set_max_position(gfx::PointF position) { max_position_ = position; }
  gfx::PointF max_position() const { return max_position_; }

  void set_proximity_range(const gfx::PointF& range) {
    proximity_range_ = range;
  }
  gfx::PointF proximity_range() const { return proximity_range_; }

 private:
  // Finds the best SnapArea candidate that's optimal for the given selection
  // strategy, while satisfying two invariants:
  // - |candidate.snap_offset| is within |cross_axis_snap_result|'s visible
  // range on |axis|.
  // - |cross_axis_snap_result.snap_offset| is within |candidate|'s visible
  // range on the cross axis.
  // |cross_axis_snap_result| is what we've found to snap on the cross axis,
  // or the original scroll offset if this is the first iteration of search.
  // Returns the candidate as SnapSearchResult that includes the area's
  // |snap_offset| and its visible range on the cross axis.
  // When |should_consider_covering| is true, the current offset can be valid if
  // it makes a snap area cover the snapport.
  // When |active_element_range| is provided, only snap areas that overlap
  // the active element are considered.
  absl::optional<SnapSearchResult> FindClosestValidAreaInternal(
      SearchAxis axis,
      const SnapSelectionStrategy& strategy,
      const SnapSearchResult& cross_axis_snap_result,
      const ElementId& active_element_id,
      bool should_consider_covering = true,
      absl::optional<gfx::RangeF> active_element_range = absl::nullopt) const;

  // A wrapper of FindClosestValidAreaInternal(). If
  // FindClosestValidAreaInternal() doesn't return a valid result when the snap
  // type is mandatory and the strategy has an intended direction, we relax the
  // strategy to ignore the direction and find again.
  absl::optional<SnapSearchResult> FindClosestValidArea(
      SearchAxis axis,
      const SnapSelectionStrategy& strategy,
      const SnapSearchResult& cross_axis_snap_result,
      const ElementId& active_element_id) const;

  bool FindSnapPositionForMutualSnap(const SnapSelectionStrategy& strategy,
                                     gfx::PointF* snap_position) const;

  // Finds the snap area associated with the target snap area element id for the
  // given axis.
  absl::optional<SnapSearchResult> GetTargetSnapAreaSearchResult(
      const SnapSelectionStrategy& strategy,
      SearchAxis axis,
      SnapSearchResult cross_axis_snap_result) const;

  // Returns all the info needed to snap at this area on the given axis,
  // including:
  // - The offset at which the snap area and the snap container meet the
  //   requested alignment.
  // - The visible range within which the snap area is visible on the cross
  //   axis.
  SnapSearchResult GetSnapSearchResult(SearchAxis axis,
                                       const SnapAreaData& data) const;

  bool IsSnapportCoveredOnAxis(SearchAxis axis,
                               float current_offset,
                               const gfx::RectF& area_rect) const;

  void UpdateSnapAreaForTesting(ElementId element_id,
                                SnapAreaData snap_area_data);

  absl::optional<SnapSearchResult> FindCoveringCandidate(
      const SnapAreaData& area,
      SearchAxis axis,
      const SnapSearchResult& aligned_candidate,
      float intended_position) const;

  // Specifies whether a scroll container is a scroll snap container, how
  // strictly it snaps, and which axes are considered.
  // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-type for details.
  ScrollSnapType scroll_snap_type_;

  // The rect of the snap_container relative to its boundary.
  gfx::RectF rect_;

  // The maximal scroll position of the SnapContainer, in the same coordinate
  // with blink's scroll position.
  gfx::PointF max_position_;

  // A valid snap position should be within the |proximity_range_| of the
  // current offset on the snapping axis.
  gfx::PointF proximity_range_;

  // The SnapAreaData for the snap areas in this snap container. When a scroll
  // happens, we iterate through the snap_area_list to find the best snap
  // position.
  std::vector<SnapAreaData> snap_area_list_;

  // Represents the ElementId(s) of the latest targeted snap areas.
  // ElementId(s) will be invalid (ElementId::kInvalidElementId) if the snap
  // container is not snapped to a position.
  TargetSnapAreaElementIds target_snap_area_element_ids_;

  FRIEND_TEST_ALL_PREFIXES(ScrollSnapDataTest, SnapToFocusedElementHorizontal);
  FRIEND_TEST_ALL_PREFIXES(ScrollSnapDataTest, SnapToFocusedElementVertical);
  FRIEND_TEST_ALL_PREFIXES(ScrollSnapDataTest, SnapToFocusedElementBoth);
};

CC_EXPORT std::ostream& operator<<(std::ostream&, const SnapAreaData&);
CC_EXPORT std::ostream& operator<<(std::ostream&, const SnapContainerData&);

}  // namespace cc

#endif  // CC_INPUT_SCROLL_SNAP_DATA_H_
