// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/filter_operations.h"

#include <stddef.h>

#include <cmath>
#include <numeric>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/features.h"
#include "cc/paint/filter_operation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

FilterOperations::FilterOperations() = default;

FilterOperations::FilterOperations(const FilterOperations& other) = default;

FilterOperations::FilterOperations(std::vector<FilterOperation>&& operations)
    : operations_(std::move(operations)) {}

FilterOperations::~FilterOperations() = default;

FilterOperations& FilterOperations::operator=(const FilterOperations& other) =
    default;

FilterOperations& FilterOperations::operator=(FilterOperations&& other) {
  operations_ = std::move(other.operations_);
  return *this;
}

bool FilterOperations::operator==(const FilterOperations& other) const {
  if (other.size() != size())
    return false;
  for (size_t i = 0; i < size(); ++i) {
    if (other.at(i) != at(i))
      return false;
  }
  return true;
}

void FilterOperations::Append(const FilterOperation& filter) {
  operations_.push_back(filter);
}

void FilterOperations::Clear() {
  operations_.clear();
}

bool FilterOperations::IsEmpty() const {
  return operations_.empty();
}

gfx::Rect FilterOperations::MapRect(const gfx::Rect& rect,
                                    const std::optional<SkMatrix>& ctm) const {
  auto accumulate_rect = [&ctm](const gfx::Rect& rect,
                                const FilterOperation& op) {
    return op.MapRect(rect, ctm);
  };
  return std::accumulate(operations_.begin(), operations_.end(), rect,
                         accumulate_rect);
}

gfx::Rect FilterOperations::MapRectReverse(const gfx::Rect& rect,
                                           const SkMatrix& ctm) const {
  auto accumulate_rect = [&ctm](const gfx::Rect& rect,
                                const FilterOperation& op) {
    return op.MapRectReverse(rect, ctm);
  };
  return std::accumulate(operations_.rbegin(), operations_.rend(), rect,
                         accumulate_rect);
}

bool FilterOperations::HasFilterThatMovesPixels() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    const FilterOperation& op = operations_[i];
    switch (op.type()) {
      case FilterOperation::BLUR:
      case FilterOperation::DROP_SHADOW:
      case FilterOperation::ZOOM:
      case FilterOperation::OFFSET:
        return true;
      case FilterOperation::REFERENCE:
        // TODO(hendrikw): SkImageFilter needs a function that tells us if the
        // filter can move pixels. See crbug.com/523538.
        return true;
      case FilterOperation::OPACITY:
      case FilterOperation::COLOR_MATRIX:
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::ALPHA_THRESHOLD:
        break;
    }
  }
  return false;
}

gfx::Rect FilterOperations::ExpandRectForPixelMovement(
    const gfx::Rect& rect) const {
  if (base::FeatureList::IsEnabled(features::kUseMapRectForPixelMovement)) {
    return MapRect(rect);
  }

  gfx::RectF expanded_rect(rect);
  expanded_rect.Outset(MaximumPixelMovement());
  return gfx::ToEnclosingRect(expanded_rect);
}

float FilterOperations::MaximumPixelMovement() const {
  float max_movement = 0.;
  for (size_t i = 0; i < operations_.size(); ++i) {
    const FilterOperation& op = operations_[i];
    switch (op.type()) {
      case FilterOperation::BLUR:
        // |op.amount| here is the blur radius.
        max_movement = fmax(max_movement, op.amount() * 3.f);
        continue;
      case FilterOperation::DROP_SHADOW:
        // |op.amount| here is the blur radius.
        max_movement = fmax(max_movement, fmax(std::abs(op.offset().x()),
                                               std::abs(op.offset().y())) +
                                              op.amount() * 3.f);
        continue;
      case FilterOperation::ZOOM:
        max_movement = fmax(max_movement, op.zoom_inset());
        continue;
      case FilterOperation::REFERENCE:
        // TODO(hendrikw): SkImageFilter needs a function that tells us how far
        // the filter can move pixels. See crbug.com/523538 (sort of).
        max_movement = fmax(max_movement, 100);
        continue;
      case FilterOperation::OFFSET:
        // TODO(crbug.com/40244221): Work out how to correctly set maximum pixel
        // movement when an offset filter may be combined with other pixel
        // moving filters.
        max_movement =
            fmax(std::abs(op.offset().x()), std::abs(op.offset().y()));
        continue;
      case FilterOperation::OPACITY:
      case FilterOperation::COLOR_MATRIX:
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::ALPHA_THRESHOLD:
        continue;
    }
  }
  return max_movement;
}

bool FilterOperations::HasFilterThatAffectsOpacity() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    const FilterOperation& op = operations_[i];
    // TODO(ajuma): Make this smarter for reference filters. Once SkImageFilter
    // can report affectsOpacity(), call that.
    switch (op.type()) {
      case FilterOperation::OPACITY:
      case FilterOperation::BLUR:
      case FilterOperation::DROP_SHADOW:
      case FilterOperation::ZOOM:
      case FilterOperation::REFERENCE:
      case FilterOperation::ALPHA_THRESHOLD:
        return true;
      case FilterOperation::COLOR_MATRIX: {
        auto& matrix = op.matrix();
        if (matrix[15] || matrix[16] || matrix[17] || matrix[18] != 1 ||
            matrix[19])
          return true;
        break;
      }
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::OFFSET:
        break;
    }
  }
  return false;
}

bool FilterOperations::HasReferenceFilter() const {
  return HasFilterOfType(FilterOperation::REFERENCE);
}

bool FilterOperations::HasFilterOfType(FilterOperation::FilterType type) const {
  return base::Contains(operations_, type, &FilterOperation::type);
}

FilterOperations FilterOperations::Blend(const FilterOperations& from,
                                         double progress) const {
  if (HasReferenceFilter() || from.HasReferenceFilter())
    return *this;

  bool from_is_longer = from.size() > size();

  size_t shorter_size, longer_size;
  if (size() == from.size()) {
    shorter_size = longer_size = size();
  } else if (from_is_longer) {
    longer_size = from.size();
    shorter_size = size();
  } else {
    longer_size = size();
    shorter_size = from.size();
  }

  for (size_t i = 0; i < shorter_size; i++) {
    if (from.at(i).type() != at(i).type())
      return *this;
  }

  FilterOperations blended_filters;
  for (size_t i = 0; i < shorter_size; i++) {
    blended_filters.Append(
        FilterOperation::Blend(&from.at(i), &at(i), progress));
  }

  if (from_is_longer) {
    for (size_t i = shorter_size; i < longer_size; i++) {
      blended_filters.Append(
          FilterOperation::Blend(&from.at(i), nullptr, progress));
    }
  } else {
    for (size_t i = shorter_size; i < longer_size; i++)
      blended_filters.Append(FilterOperation::Blend(nullptr, &at(i), progress));
  }

  return blended_filters;
}

void FilterOperations::AsValueInto(
    base::trace_event::TracedValue* value) const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    value->BeginDictionary();
    operations_[i].AsValueInto(value);
    value->EndDictionary();
  }
}

std::string FilterOperations::ToString() const {
  base::trace_event::TracedValueJSON value;
  value.BeginArray("FilterOperations");
  AsValueInto(&value);
  value.EndArray();
  return value.ToJSON();
}

}  // namespace cc
