// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace ash {

FakeScannerProfileScopedDelegate::FakeScannerProfileScopedDelegate() = default;

FakeScannerProfileScopedDelegate::~FakeScannerProfileScopedDelegate() = default;

ScannerSystemState FakeScannerProfileScopedDelegate::GetSystemState() const {
  return ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{});
}

void FakeScannerProfileScopedDelegate::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    base::OnceCallback<void(ScannerActionsResponse)> callback) {
  std::move(callback).Run(base::ok(std::vector<ScannerAction>{
      ScannerAction(
          /*display_name=*/"Open Url",
          /*command=*/OpenUrlCommand{.url = GURL("https://www.google.com")}),
  }));
}

}  // namespace ash
