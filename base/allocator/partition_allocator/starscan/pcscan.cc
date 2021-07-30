// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan.h"

#include "base/allocator/partition_allocator/starscan/pcscan_internal.h"

namespace base {
namespace internal {

void PCScan::Initialize(WantedWriteProtectionMode wpmode) {
  PCScanInternal::Instance().Initialize(wpmode);
}

void PCScan::RegisterScannableRoot(Root* root) {
  PCScanInternal::Instance().RegisterScannableRoot(root);
}

void PCScan::RegisterNonScannableRoot(Root* root) {
  PCScanInternal::Instance().RegisterNonScannableRoot(root);
}

void PCScan::RegisterNewSuperPage(Root* root, uintptr_t super_page_base) {
  PCScanInternal::Instance().RegisterNewSuperPage(root, super_page_base);
}

void PCScan::PerformScan(InvocationMode invocation_mode) {
  PCScanInternal::Instance().PerformScan(invocation_mode);
}

void PCScan::PerformScanIfNeeded(InvocationMode invocation_mode) {
  PCScanInternal::Instance().PerformScanIfNeeded(invocation_mode);
}

void PCScan::PerformDelayedScan(TimeDelta delay) {
  PCScanInternal::Instance().PerformDelayedScan(delay);
}

void PCScan::JoinScan() {
  PCScanInternal::Instance().JoinScan();
}

void PCScan::SetProcessName(const char* process_name) {
  PCScanInternal::Instance().SetProcessName(process_name);
}

void PCScan::EnableStackScanning() {
  PCScanInternal::Instance().EnableStackScanning();
}
void PCScan::DisableStackScanning() {
  PCScanInternal::Instance().DisableStackScanning();
}
bool PCScan::IsStackScanningEnabled() {
  return PCScanInternal::Instance().IsStackScanningEnabled();
}

void PCScan::EnableImmediateFreeing() {
  PCScanInternal::Instance().EnableImmediateFreeing();
}

void PCScan::NotifyThreadCreated(void* stack_top) {
  PCScanInternal::Instance().NotifyThreadCreated(stack_top);
}
void PCScan::NotifyThreadDestroyed() {
  PCScanInternal::Instance().NotifyThreadDestroyed();
}

void PCScan::SetClearType(ClearType clear_type) {
  PCScan& instance = Instance();
  instance.clear_type_ = clear_type;
}

void PCScan::UninitForTesting() {
  PCScanInternal::Instance().ClearRootsForTesting();  // IN-TEST
  ReinitPCScanMetadataAllocatorForTesting();          // IN-TEST
}

void PCScan::ReinitForTesting(WantedWriteProtectionMode wpmode) {
  PCScanInternal::Instance().ReinitForTesting(wpmode);  // IN-TEST
}

void PCScan::FinishScanForTesting() {
  PCScanInternal::Instance().FinishScanForTesting();  // IN-TEST
}

PCScan PCScan::instance_ CONSTINIT;

}  // namespace internal
}  // namespace base
