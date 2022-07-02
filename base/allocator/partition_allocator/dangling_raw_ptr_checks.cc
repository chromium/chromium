// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"

namespace partition_alloc {

namespace {
DanglingRawPtrDetectedFn* g_dangling_raw_ptr_detected_fn = [](uintptr_t) {};
DanglingRawPtrReleasedFn* g_dangling_raw_ptr_released_fn = [](uintptr_t) {};
}  // namespace

DanglingRawPtrDetectedFn* GetDanglingRawPtrDetectedFn() {
  PA_DCHECK(g_dangling_raw_ptr_detected_fn);
  return g_dangling_raw_ptr_detected_fn;
}

DanglingRawPtrDetectedFn* GetDanglingRawPtrReleasedFn() {
  PA_DCHECK(g_dangling_raw_ptr_released_fn);
  return g_dangling_raw_ptr_released_fn;
}

void SetDanglingRawPtrDetectedFn(DanglingRawPtrDetectedFn fn) {
  PA_DCHECK(fn);
  g_dangling_raw_ptr_detected_fn = fn;
}

void SetDanglingRawPtrReleasedFn(DanglingRawPtrReleasedFn fn) {
  PA_DCHECK(fn);
  g_dangling_raw_ptr_released_fn = fn;
}

namespace internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC) void DanglingRawPtrDetected(uintptr_t id) {
  g_dangling_raw_ptr_detected_fn(id);
}
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void DanglingRawPtrReleased(uintptr_t id) {
  g_dangling_raw_ptr_released_fn(id);
}

}  // namespace internal
}  // namespace partition_alloc
