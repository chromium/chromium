// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_delegate.h"

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

namespace ash {

FakeScannerDelegate::FakeScannerDelegate() = default;

FakeScannerDelegate::~FakeScannerDelegate() = default;

ScannerProfileScopedDelegate* FakeScannerDelegate::GetProfileScopedDelegate() {
  return &fake_scanner_profile_scoped_delegate_;
}

}  // namespace ash
