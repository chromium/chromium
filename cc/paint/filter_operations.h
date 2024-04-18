// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_FILTER_OPERATIONS_H_
#define CC_PAINT_FILTER_OPERATIONS_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "cc/paint/filter_operation.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace gfx {
class Rect;
}

namespace cc {

// An ordered list of filter operations.
class CC_PAINT_EXPORT FilterOperations {
 public:
  FilterOperations();

  FilterOperations(const FilterOperations& other);

  explicit FilterOperations(std::vector<FilterOperation>&& operations);

  ~FilterOperations();

  FilterOperations& operator=(const FilterOperations& other);

  FilterOperations& operator=(FilterOperations&& other);

  bool operator==(const FilterOperations& other) const;

  bool operator!=(const FilterOperations& other) const {
    return !(*this == other);
  }

  void Append(const FilterOperation& filter);

  // Removes all filter operations.
  void Clear();

  bool IsEmpty() const;

  // Maps "forward" to determine which pixels in a destination rect are affected
  // by pixels in the source rect. See PaintFilter::MapRect() about `ctm`.
  gfx::Rect MapRect(const gfx::Rect& rect,
                    const std::optional<SkMatrix>& matrix = std::nullopt) const;

  // Maps "backward" to determine which pixels in the source affect the pixels
  // in the destination rect. See PaintFilter::MapRect() about `ctm`.
  gfx::Rect MapRectReverse(const gfx::Rect& rect, const SkMatrix& matrix) const;

  bool HasFilterThatMovesPixels() const;

  // Expands `rect` to add any additional area that applying pixel moving
  // filters will modify.
  gfx::Rect ExpandRectForPixelMovement(const gfx::Rect& rect) const;

  bool HasFilterThatAffectsOpacity() const;
  bool HasReferenceFilter() const;
  bool HasFilterOfType(FilterOperation::FilterType type) const;

  size_t size() const { return operations_.size(); }

  const std::vector<FilterOperation>& operations() const { return operations_; }

  const FilterOperation& at(size_t index) const {
    DCHECK_LT(index, size());
    return operations_[index];
  }

  // If |from| is of the same size as this, where in each position, the filter
  // in |from| is of the same type as the filter in this, and if this doesn't
  // contain any reference filters, returns a FilterOperations formed by
  // linearly interpolating at each position a |progress| fraction of the way
  // from the filter in |from| to the filter in this. If |from| and this are of
  // different lengths, they are treated as having the same length by padding
  // the shorter sequence with no-op filters of the same type as the filters in
  // the corresponding positions in the longer sequence. If either sequence has
  // a reference filter or if there is a type mismatch at some position, returns
  // a copy of this.
  FilterOperations Blend(const FilterOperations& from, double progress) const;

  void AsValueInto(base::trace_event::TracedValue* value) const;
  std::string ToString() const;

 private:
  float MaximumPixelMovement() const;

  std::vector<FilterOperation> operations_;
};

}  // namespace cc

#endif  // CC_PAINT_FILTER_OPERATIONS_H_
