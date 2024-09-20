// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNER_CHROME_SCANNER_DELEGATE_H_
#define CHROME_BROWSER_ASH_SCANNER_CHROME_SCANNER_DELEGATE_H_

#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "base/memory/raw_ptr.h"

namespace ash {
class ScannerProfileScopedDelegate;
}  // namespace ash

// Provides Chrome browser access to //ash/scanner.
class ChromeScannerDelegate : public ash::ScannerDelegate {
 public:
  ChromeScannerDelegate();
  ChromeScannerDelegate(const ChromeScannerDelegate&) = delete;
  ChromeScannerDelegate& operator=(const ChromeScannerDelegate&) = delete;
  ~ChromeScannerDelegate() override;

  // ash::ScannerDelegate:
  ash::ScannerProfileScopedDelegate* GetProfileScopedDelegate() override;
};

#endif  // CHROME_BROWSER_ASH_SCANNER_CHROME_SCANNER_DELEGATE_H_
