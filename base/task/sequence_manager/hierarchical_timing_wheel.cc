// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/hierarchical_timing_wheel.h"

namespace base::sequence_manager {

////////////////////////////////////////////////////////////////////////////////
// HierarchicalTimingWheelHandle

HierarchicalTimingWheelHandle::HierarchicalTimingWheelHandle() = default;

HierarchicalTimingWheelHandle::HierarchicalTimingWheelHandle(
    HierarchicalTimingWheelHandle&& other) noexcept
    : timing_wheel_handle_(std::move(other.timing_wheel_handle_)),
      heap_handle_(std::move(other.heap_handle_)),
      hierarchy_index_(std::exchange(other.hierarchy_index_, kInvalidIndex)) {}

HierarchicalTimingWheelHandle& HierarchicalTimingWheelHandle::operator=(
    HierarchicalTimingWheelHandle&& other) noexcept {
  timing_wheel_handle_ = std::move(other.timing_wheel_handle_);
  heap_handle_ = std::move(other.heap_handle_);
  hierarchy_index_ = std::exchange(other.hierarchy_index_, kInvalidIndex);
  return *this;
}

HierarchicalTimingWheelHandle::~HierarchicalTimingWheelHandle() = default;

internal::TimingWheelHandle
HierarchicalTimingWheelHandle::GetTimingWheelHandle() const {
  return timing_wheel_handle_;
}

void HierarchicalTimingWheelHandle::SetTimingWheelHandle(
    internal::TimingWheelHandle timing_wheel_handle) {
  DCHECK(timing_wheel_handle.IsValid());
  DCHECK(!heap_handle_.IsValid());
  timing_wheel_handle_ = timing_wheel_handle;
}

void HierarchicalTimingWheelHandle::ClearTimingWheelHandle() {
  timing_wheel_handle_.Reset();
}

HeapHandle HierarchicalTimingWheelHandle::GetHeapHandle() {
  return heap_handle_;
}

void HierarchicalTimingWheelHandle::SetHeapHandle(HeapHandle heap_handle) {
  DCHECK(heap_handle.IsValid());
  DCHECK(!timing_wheel_handle_.IsValid());
  heap_handle_ = heap_handle;
}

void HierarchicalTimingWheelHandle::ClearHeapHandle() {
  heap_handle_.reset();
}

size_t HierarchicalTimingWheelHandle::GetHierarchyIndex() const {
  return hierarchy_index_;
}

void HierarchicalTimingWheelHandle::SetHierarchyIndex(size_t hierarchy_index) {
  DCHECK(hierarchy_index != kInvalidIndex);
  hierarchy_index_ = hierarchy_index;
}

void HierarchicalTimingWheelHandle::ClearHierarchyIndex() {
  hierarchy_index_ = kInvalidIndex;
}

// static
HierarchicalTimingWheelHandle HierarchicalTimingWheelHandle::Invalid() {
  return HierarchicalTimingWheelHandle();
}

bool HierarchicalTimingWheelHandle::IsValid() const {
  return (timing_wheel_handle_.IsValid() || heap_handle_.IsValid()) &&
         hierarchy_index_ != kInvalidIndex;
}

}  // namespace base::sequence_manager
