// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_FAKE_SCANNER_DELEGATE_H_
#define ASH_SCANNER_FAKE_SCANNER_DELEGATE_H_

#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

namespace ash {

// A fake ScannerDelegate which can be used in tests.
class FakeScannerDelegate : public ScannerDelegate {
 public:
  FakeScannerDelegate();
  FakeScannerDelegate(const FakeScannerDelegate&) = delete;
  FakeScannerDelegate& operator=(const FakeScannerDelegate&) = delete;
  ~FakeScannerDelegate() override;

  // ScannerDelegate:
  ScannerProfileScopedDelegate* GetProfileScopedDelegate() override;

 private:
  FakeScannerProfileScopedDelegate fake_scanner_profile_scoped_delegate_;
};

}  // namespace ash

#endif  // ASH_SCANNER_FAKE_SCANNER_DELEGATE_H_
