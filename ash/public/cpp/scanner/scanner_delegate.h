// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ScannerProfileScopedDelegate;

// Provides access to the browser from //ash/scanner.
class ASH_PUBLIC_EXPORT ScannerDelegate {
 public:
  virtual ~ScannerDelegate() = default;

  virtual ScannerProfileScopedDelegate* GetProfileScopedDelegate() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_
