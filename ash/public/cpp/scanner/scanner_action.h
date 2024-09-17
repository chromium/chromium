// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// This holds a particular action the user can complete in a ScannerSession. An
// action is a single operation that can be applied to the system.
class ASH_PUBLIC_EXPORT ScannerAction {
 public:
  virtual ~ScannerAction() = default;

  // Completes the operation contained within this action.
  virtual void Execute() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
