// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/test/memory/dangling_ptr_instrumentation.h"

#include <cstdint>
#include <string_view>

#include "base/allocator/partition_alloc_features.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

// static
base::expected<DanglingPtrInstrumentation, std::string_view>
DanglingPtrInstrumentation::Create() {
  if (!FeatureList::IsEnabled(features::kPartitionAllocBackupRefPtr)) {
    return base::unexpected(
        "DanglingPtrInstrumentation requires the feature flag "
        "'PartitionAllocBackupRefPtr' to be on.");
  }
  // Note: We don't need to enable the `PartitionAllocDanglingPtr` feature,
  // because this does provide an alternative "implementation", by incrementing
  // the two counters.

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return base::unexpected(
      "DanglingPtrInstrumentation requires the binary flag "
      "'use_partition_alloc_as_malloc' to be on.");
#elif !PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  return base::unexpected(
      "DanglingPtrInstrumentation requires the binary flag "
      "'enable_dangling_raw_ptr_checks' to be on.");
#else
  return DanglingPtrInstrumentation();
#endif
}

DanglingPtrInstrumentation::DanglingPtrInstrumentation() {
  Register();
}

DanglingPtrInstrumentation::~DanglingPtrInstrumentation() {
  Unregister();
}

DanglingPtrInstrumentation::DanglingPtrInstrumentation(
    DanglingPtrInstrumentation&& old) {
  operator=(std::move(old));
}

DanglingPtrInstrumentation& DanglingPtrInstrumentation::operator=(
    DanglingPtrInstrumentation&& old) {
  old.Unregister();
  Register();
  return *this;
}

void DanglingPtrInstrumentation::Register() {
  CHECK_EQ(g_observer, nullptr);
  g_observer = this;
  old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
  old_dereferenced_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();
  partition_alloc::SetDanglingRawPtrDetectedFn(IncreaseCountDetected);
  partition_alloc::SetDanglingRawPtrReleasedFn(IncreaseCountReleased);
}

void DanglingPtrInstrumentation::Unregister() {
  if (g_observer != this) {
    return;
  }
  g_observer = nullptr;
  partition_alloc::SetDanglingRawPtrDetectedFn(old_detected_fn_);
  partition_alloc::SetDanglingRawPtrReleasedFn(old_dereferenced_fn_);
}

raw_ptr<DanglingPtrInstrumentation> DanglingPtrInstrumentation::g_observer =
    nullptr;

// static
void DanglingPtrInstrumentation::IncreaseCountDetected(std::uintptr_t) {
  g_observer->dangling_ptr_detected_++;
}

// static
void DanglingPtrInstrumentation::IncreaseCountReleased(std::uintptr_t) {
  g_observer->dangling_ptr_released_++;
}

}  // namespace base::test
