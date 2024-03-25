// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/starscan/pcscan.h"

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/starscan/pcscan_internal.h"

namespace partition_alloc::internal {

void PCScan::Initialize(InitConfig config) {
  PCScanInternal::Instance().Initialize(config);
}

bool PCScan::IsInitialized() {
  return PCScanInternal::Instance().is_initialized();
}

void PCScan::Disable() {
  auto& instance = PCScan::Instance();
  instance.scheduler().scheduling_backend().DisableScheduling();
}

bool PCScan::IsEnabled() {
  auto& instance = PCScan::Instance();
  return instance.scheduler().scheduling_backend().is_scheduling_enabled();
}

void PCScan::Reenable() {
  auto& instance = PCScan::Instance();
  instance.scheduler().scheduling_backend().EnableScheduling();
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

void PCScan::PerformDelayedScan(int64_t delay_in_microseconds) {
  PCScanInternal::Instance().PerformDelayedScan(
      base::Microseconds(delay_in_microseconds));
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

void PCScan::SetClearType(ClearType clear_type) {
  PCScan& instance = Instance();
  instance.clear_type_ = clear_type;
}

void PCScan::UninitForTesting() {
  PCScanInternal::Instance().ClearRootsForTesting();  // IN-TEST
}

void PCScan::ReinitForTesting(InitConfig config) {
  PCScanInternal::Instance().ReinitForTesting(config);  // IN-TEST
}

void PCScan::FinishScanForTesting() {
  PCScanInternal::Instance().FinishScanForTesting();  // IN-TEST
}

void PCScan::RegisterStatsReporter(partition_alloc::StatsReporter* reporter) {
  PCScanInternal::Instance().RegisterStatsReporter(reporter);
}

PCScan PCScan::instance_ PA_CONSTINIT;

}  // namespace partition_alloc::internal
