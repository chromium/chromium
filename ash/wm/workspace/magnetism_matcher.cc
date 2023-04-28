// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/magnetism_matcher.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"

namespace ash {
namespace {

// Returns true if |a| is close enough to |b| that the two edges snap.
bool IsCloseEnough(int a, int b) {
  return abs(a - b) <= MagnetismMatcher::kMagneticDistance;
}

// Returns true if the specified SecondaryMagnetismEdge can be matched with a
// primary edge of |primary|. |edges| is a bitmask of the allowed
// MagnetismEdges.
bool CanMatchSecondaryEdge(MagnetismEdge primary,
                           SecondaryMagnetismEdge secondary,
                           uint32_t edges) {
  // Convert |secondary| to a MagnetismEdge so we can compare it to |edges|.
  MagnetismEdge secondary_as_magnetism_edge = MAGNETISM_EDGE_TOP;
  switch (primary) {
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      if (secondary == SECONDARY_MAGNETISM_EDGE_LEADING)
        secondary_as_magnetism_edge = MAGNETISM_EDGE_LEFT;
      else if (secondary == SECONDARY_MAGNETISM_EDGE_TRAILING)
        secondary_as_magnetism_edge = MAGNETISM_EDGE_RIGHT;
      else
        NOTREACHED();
      break;
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      if (secondary == SECONDARY_MAGNETISM_EDGE_LEADING)
        secondary_as_magnetism_edge = MAGNETISM_EDGE_TOP;
      else if (secondary == SECONDARY_MAGNETISM_EDGE_TRAILING)
        secondary_as_magnetism_edge = MAGNETISM_EDGE_BOTTOM;
      else
        NOTREACHED();
      break;
  }
  return (edges & secondary_as_magnetism_edge) != 0;
}

}  // namespace

// MagnetismEdgeMatcher --------------------------------------------------------

MagnetismEdgeMatcher::MagnetismEdgeMatcher(const gfx::Rect& bounds,
                                           MagnetismEdge edge)
    : bounds_(bounds), edge_(edge) {
  ranges_.push_back(GetSecondaryRange(bounds_));
}

MagnetismEdgeMatcher::~MagnetismEdgeMatcher() = default;

bool MagnetismEdgeMatcher::ShouldAttach(const gfx::Rect& bounds) {
  if (is_edge_obscured())
    return false;

  if (IsCloseEnough(GetPrimaryCoordinate(bounds_, edge_),
                    GetPrimaryCoordinate(bounds, FlipEdge(edge_)))) {
    const Range range(GetSecondaryRange(bounds));
    Ranges::const_iterator i =
        std::lower_bound(ranges_.begin(), ranges_.end(), range);
    // Close enough, but only attach if some portion of the edge is visible.
    if ((i != ranges_.begin() && RangesIntersect(*(i - 1), range)) ||
        (i != ranges_.end() && RangesIntersect(*i, range))) {
      return true;
    }
  }
  // NOTE: this checks against the current bounds, we may want to allow some
  // flexibility here.
  const Range primary_range(GetPrimaryRange(bounds));
  if (primary_range.first <= GetPrimaryCoordinate(bounds_, edge_) &&
      primary_range.second >= GetPrimaryCoordinate(bounds_, edge_)) {
    UpdateRanges(GetSecondaryRange(bounds));
  }
  return false;
}

void MagnetismEdgeMatcher::UpdateRanges(const Range& range) {
  Ranges::const_iterator it =
      std::lower_bound(ranges_.begin(), ranges_.end(), range);
  if (it != ranges_.begin() && RangesIntersect(*(it - 1), range))
    --it;
  if (it == ranges_.end())
    return;

  for (size_t i = it - ranges_.begin();
       i < ranges_.size() && RangesIntersect(ranges_[i], range);) {
    if (range.first <= ranges_[i].first && range.second >= ranges_[i].second) {
      ranges_.erase(ranges_.begin() + i);
    } else if (range.first < ranges_[i].first) {
      DCHECK_GT(range.second, ranges_[i].first);
      ranges_[i] = Range(range.second, ranges_[i].second);
      ++i;
    } else {
      Range existing(ranges_[i]);
      ranges_[i].second = range.first;
      ++i;
      if (existing.second > range.second) {
        ranges_.insert(ranges_.begin() + i,
                       Range(range.second, existing.second));
        ++i;
      }
    }
  }
}

// MagnetismMatcher ------------------------------------------------------------

// static
const int MagnetismMatcher::kMagneticDistance = 8;

MagnetismMatcher::MagnetismMatcher(const gfx::Rect& bounds, uint32_t edges)
    : edges_(edges) {
  if (edges & MAGNETISM_EDGE_TOP) {
    matchers_.push_back(
        std::make_unique<MagnetismEdgeMatcher>(bounds, MAGNETISM_EDGE_TOP));
  }
  if (edges & MAGNETISM_EDGE_LEFT) {
    matchers_.push_back(
        std::make_unique<MagnetismEdgeMatcher>(bounds, MAGNETISM_EDGE_LEFT));
  }
  if (edges & MAGNETISM_EDGE_BOTTOM) {
    matchers_.push_back(
        std::make_unique<MagnetismEdgeMatcher>(bounds, MAGNETISM_EDGE_BOTTOM));
  }
  if (edges & MAGNETISM_EDGE_RIGHT) {
    matchers_.push_back(
        std::make_unique<MagnetismEdgeMatcher>(bounds, MAGNETISM_EDGE_RIGHT));
  }
}

MagnetismMatcher::~MagnetismMatcher() = default;

bool MagnetismMatcher::ShouldAttach(const gfx::Rect& bounds,
                                    MatchedEdge* edge) {
  for (const auto& matcher : matchers_) {
    if (matcher->ShouldAttach(bounds)) {
      edge->primary_edge = matcher->edge();
      AttachToSecondaryEdge(bounds, edge->primary_edge,
                            &(edge->secondary_edge));
      return true;
    }
  }
  return false;
}

bool MagnetismMatcher::AreEdgesObscured() const {
  for (const auto& matcher : matchers_) {
    if (!matcher->is_edge_obscured())
      return false;
  }
  return true;
}

void MagnetismMatcher::AttachToSecondaryEdge(
    const gfx::Rect& bounds,
    MagnetismEdge edge,
    SecondaryMagnetismEdge* secondary_edge) const {
  const gfx::Rect& src_bounds(matchers_[0]->bounds());
  if (edge == MAGNETISM_EDGE_LEFT || edge == MAGNETISM_EDGE_RIGHT) {
    if (CanMatchSecondaryEdge(edge, SECONDARY_MAGNETISM_EDGE_LEADING, edges_) &&
        IsCloseEnough(bounds.y(), src_bounds.y())) {
      *secondary_edge = SECONDARY_MAGNETISM_EDGE_LEADING;
    } else if (CanMatchSecondaryEdge(edge, SECONDARY_MAGNETISM_EDGE_TRAILING,
                                     edges_) &&
               IsCloseEnough(bounds.bottom(), src_bounds.bottom())) {
      *secondary_edge = SECONDARY_MAGNETISM_EDGE_TRAILING;
    } else {
      *secondary_edge = SECONDARY_MAGNETISM_EDGE_NONE;
    }
  } else {
    if (CanMatchSecondaryEdge(edge, SECONDARY_MAGNETISM_EDGE_LEADING, edges_) &&
        IsCloseEnough(bounds.x(), src_bounds.x())) {
      *secondary_edge = SECONDARY_MAGNETISM_EDGE_LEADING;
    } else if (CanMatchSecondaryEdge(edge, SECONDARY_MAGNETISM_EDGE_TRAILING,
                                     edges_) &&
               IsCloseEnough(bounds.right(), src_bounds.right())) {
      *secondary_edge = SECONDARY_MAGNETISM_EDGE_TRAILING;
    } else {
      *secondary_edge = SECONDARY_MAGNETISM_EDGE_NONE;
    }
  }
}

}  // namespace ash
