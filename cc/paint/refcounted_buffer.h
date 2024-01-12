// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_REFCOUNTED_BUFFER_H_
#define CC_PAINT_REFCOUNTED_BUFFER_H_

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"

namespace cc {

// A trivial RefCounted wrapper for a block of data.
// This is intended to minimize the number of copies when e.g.
// recording large vertex/uv/index arrays to a PaintOpBuffer.
template <typename T>
class CC_PAINT_EXPORT RefCountedBuffer
    : public base::RefCounted<RefCountedBuffer<T>> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit RefCountedBuffer(std::vector<T> data) : buffer_(std::move(data)) {}

  const std::vector<T>& data() const { return buffer_; }
  std::vector<T>& data() { return buffer_; }

  bool operator==(const RefCountedBuffer& other) const {
    return buffer_ == other.buffer_;
  }

 private:
  friend class base::RefCounted<RefCountedBuffer<T>>;
  ~RefCountedBuffer() = default;

  std::vector<T> buffer_;
};

}  // namespace cc

#endif  // CC_PAINT_REFCOUNTED_BUFFER_H_
