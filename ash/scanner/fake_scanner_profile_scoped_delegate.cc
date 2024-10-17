// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "base/check.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"

namespace ash {

FakeScannerProfileScopedDelegate::FakeScannerProfileScopedDelegate() = default;

FakeScannerProfileScopedDelegate::~FakeScannerProfileScopedDelegate() = default;

ScannerSystemState FakeScannerProfileScopedDelegate::GetSystemState() const {
  return ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{});
}

void FakeScannerProfileScopedDelegate::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    manta::ScannerProvider::ScannerProtoResponseCallback callback) {
  fetch_actions_callback_ = std::move(callback);
}

void FakeScannerProfileScopedDelegate::SendFakeActionsResponse(
    std::unique_ptr<manta::proto::ScannerOutput> output,
    manta::MantaStatus status) {
  CHECK(!fetch_actions_callback_.is_null());
  std::move(fetch_actions_callback_).Run(std::move(output), std::move(status));
}

drive::DriveServiceInterface*
FakeScannerProfileScopedDelegate::GetDriveService() {
  return &drive_service_;
}

}  // namespace ash
