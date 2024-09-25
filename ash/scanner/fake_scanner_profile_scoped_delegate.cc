// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"

namespace ash {

FakeScannerProfileScopedDelegate::FakeScannerProfileScopedDelegate() = default;

FakeScannerProfileScopedDelegate::~FakeScannerProfileScopedDelegate() = default;

ScannerSystemState FakeScannerProfileScopedDelegate::GetSystemState() const {
  return ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{});
}

void FakeScannerProfileScopedDelegate::FetchActions(
    base::OnceCallback<void(ScannerActionsResponse)> callback) {
  std::move(callback).Run(base::ok(std::vector<ScannerAction>()));
}

}  // namespace ash
