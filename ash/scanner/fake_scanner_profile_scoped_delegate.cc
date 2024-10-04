// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

#include <utility>

#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

FakeScannerProfileScopedDelegate::FakeScannerProfileScopedDelegate() = default;

FakeScannerProfileScopedDelegate::~FakeScannerProfileScopedDelegate() = default;

ScannerSystemState FakeScannerProfileScopedDelegate::GetSystemState() const {
  return ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{});
}

void FakeScannerProfileScopedDelegate::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    base::OnceCallback<void(ScannerActionsResponse)> callback) {
  fetch_actions_callback_ = std::move(callback);
}

void FakeScannerProfileScopedDelegate::SendFakeActionsResponse(
    ScannerActionsResponse actions_response) {
  CHECK(!fetch_actions_callback_.is_null());
  std::move(fetch_actions_callback_).Run(actions_response);
}

}  // namespace ash
