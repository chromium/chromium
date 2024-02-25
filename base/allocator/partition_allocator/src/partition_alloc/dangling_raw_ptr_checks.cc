// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/dangling_raw_ptr_checks.h"

#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_check.h"

namespace partition_alloc {

namespace {
DanglingRawPtrDetectedFn* g_dangling_raw_ptr_detected_fn = [](uintptr_t) {};
DanglingRawPtrReleasedFn* g_dangling_raw_ptr_released_fn = [](uintptr_t) {};
DanglingRawPtrDetectedFn* g_unretained_dangling_raw_ptr_detected_fn =
    [](uintptr_t) {};
bool g_unretained_dangling_raw_ptr_check_enabled = false;
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

DanglingRawPtrDetectedFn* GetUnretainedDanglingRawPtrDetectedFn() {
  return g_unretained_dangling_raw_ptr_detected_fn;
}

void SetUnretainedDanglingRawPtrDetectedFn(DanglingRawPtrDetectedFn* fn) {
  PA_DCHECK(fn);
  g_unretained_dangling_raw_ptr_detected_fn = fn;
}

bool SetUnretainedDanglingRawPtrCheckEnabled(bool enabled) {
  bool old = g_unretained_dangling_raw_ptr_check_enabled;
  g_unretained_dangling_raw_ptr_check_enabled = enabled;
  return old;
}

namespace internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC) void DanglingRawPtrDetected(uintptr_t id) {
  g_dangling_raw_ptr_detected_fn(id);
}
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void DanglingRawPtrReleased(uintptr_t id) {
  g_dangling_raw_ptr_released_fn(id);
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void UnretainedDanglingRawPtrDetected(uintptr_t id) {
  g_unretained_dangling_raw_ptr_detected_fn(id);
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool IsUnretainedDanglingRawPtrCheckEnabled() {
  return g_unretained_dangling_raw_ptr_check_enabled;
}

}  // namespace internal
}  // namespace partition_alloc
