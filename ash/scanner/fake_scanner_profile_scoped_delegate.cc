// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "components/drive/service/drive_service_interface.h"

namespace ash {

FakeScannerProfileScopedDelegate::FakeScannerProfileScopedDelegate() = default;

FakeScannerProfileScopedDelegate::~FakeScannerProfileScopedDelegate() = default;

ScannerSystemState FakeScannerProfileScopedDelegate::GetSystemState() const {
  return ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{});
}

drive::DriveServiceInterface*
FakeScannerProfileScopedDelegate::GetDriveService() {
  return &drive_service_;
}

}  // namespace ash
