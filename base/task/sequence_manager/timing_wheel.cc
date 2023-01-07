// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/timing_wheel.h"

namespace base::sequence_manager::internal {

////////////////////////////////////////////////////////////////////////////////
// TimingWheelHandle

TimingWheelHandle::TimingWheelHandle(TimingWheelHandle&& other) noexcept
    : bucket_index_(std::exchange(other.bucket_index_, kInvalidIndex)),
      element_index_(std::exchange(other.element_index_, kInvalidIndex)) {}

TimingWheelHandle& TimingWheelHandle::operator=(
    TimingWheelHandle&& other) noexcept {
  bucket_index_ = std::exchange(other.bucket_index_, kInvalidIndex);
  element_index_ = std::exchange(other.element_index_, kInvalidIndex);
  return *this;
}

// static
TimingWheelHandle TimingWheelHandle::Invalid() {
  return TimingWheelHandle();
}

void TimingWheelHandle::Reset() {
  bucket_index_ = kInvalidIndex;
  element_index_ = kInvalidIndex;
}

bool TimingWheelHandle::IsValid() const {
  return bucket_index_ != kInvalidIndex && element_index_ != kInvalidIndex;
}

size_t TimingWheelHandle::bucket_index() const {
  return bucket_index_;
}

size_t TimingWheelHandle::element_index() const {
  return element_index_;
}

TimingWheelHandle::TimingWheelHandle(size_t bucket_index, size_t element_index)
    : bucket_index_(bucket_index), element_index_(element_index) {}

}  // namespace base::sequence_manager::internal
