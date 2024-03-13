// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_READER_H_
#define BASE_CONTAINERS_SPAN_READER_H_

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

namespace base {

// A Reader to consume elements from the front of a span dynamically.
//
// SpanReader is used to split off prefix spans from a larger span, reporting
// errors if there's not enough room left (instead of crashing, as would happen
// with span directly).
template <class T>
class SpanReader {
 public:
  // Construct SpanReader from a span.
  explicit SpanReader(span<T> buf) : buf_(buf) {}

  // Returns a span over the next `n` objects, if there are enough objects left.
  // Otherwise, it returns nullopt and does nothing.
  std::optional<span<T>> Read(base::StrictNumeric<size_t> n) {
    if (n > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.split_at(n);
    buf_ = rhs;
    return lhs;
  }

  // Returns a fixed-size span over the next `N` objects, if there are enough
  // objects left. Otherwise, it returns nullopt and does nothing.
  template <size_t N>
  std::optional<span<T, N>> Read() {
    if (N > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.template split_at<N>();
    buf_ = rhs;
    return lhs;
  }

  // Returns true and writes a span over the next `n` objects into `out`, if
  // there are enough objects left. Otherwise, it returns false and does
  // nothing.
  bool ReadInto(base::StrictNumeric<size_t> n, span<T>& out) {
    if (n > remaining()) {
      return false;
    }
    auto [lhs, rhs] = buf_.split_at(n);
    out = lhs;
    buf_ = rhs;
    return true;
  }

  // Returns true and skips over the next `n` objects, if there are enough
  // objects left. Otherwise, it returns false and does nothing.
  bool Skip(base::StrictNumeric<size_t> n) {
    if (n > remaining()) {
      return false;
    }
    buf_ = buf_.subspan(n);
    return true;
  }

  // Returns the number of objects remaining to be read from the original span.
  size_t remaining() const { return buf_.size(); }

  // Returns the objects that have not yet been read, as a span.
  span<T> remaining_span() const { return buf_; }

 private:
  span<T> buf_;
};

template <class T, size_t N>
SpanReader(span<T, N>) -> SpanReader<T>;

}  // namespace base

#endif  // BASE_CONTAINERS_SPAN_READER_H_
