// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_
#define ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_

#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"

namespace ash {

// A fake ScannerProfileScopedDelegate which can be used in tests.
class FakeScannerProfileScopedDelegate : public ScannerProfileScopedDelegate {
 public:
  FakeScannerProfileScopedDelegate();
  FakeScannerProfileScopedDelegate(const FakeScannerProfileScopedDelegate&) =
      delete;
  FakeScannerProfileScopedDelegate& operator=(
      const FakeScannerProfileScopedDelegate&) = delete;
  ~FakeScannerProfileScopedDelegate() override;

  // ScannerProfileScopedDelegate:
  ScannerSystemState GetSystemState() const override;
  void FetchActions(
      base::OnceCallback<void(ScannerActionsResponse)> callback) override;
};

}  // namespace ash

#endif  // ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_
