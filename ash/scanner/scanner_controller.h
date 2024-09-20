// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_CONTROLLER_H_
#define ASH_SCANNER_SCANNER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class ScannerDelegate;
class ScannerSession;

// This is the top level controller used for Scanner. It acts as a mediator
// between Scanner and any consuming features.
class ASH_EXPORT ScannerController {
 public:
  explicit ScannerController(std::unique_ptr<ScannerDelegate> delegate);
  ScannerController(const ScannerController&) = delete;
  ScannerController& operator=(const ScannerController&) = delete;
  ~ScannerController();

  static bool IsEnabled();

  // As the name suggests this function creates a new ScannerSession. A
  // ScannerSession will exist for the entire time a user is interacting with
  // the feature. Triggering features should use this method to initiate a new
  // ScannerSession. If nullptr is returned then Scanner cannot be initialized
  // due to system level constraints (i.e. pref disabled, feature not allowed).
  std::unique_ptr<ScannerSession> StartNewSession();

 private:
  std::unique_ptr<ScannerDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_CONTROLLER_H_
