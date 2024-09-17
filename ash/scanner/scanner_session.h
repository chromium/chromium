// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_SESSION_H_
#define ASH_SCANNER_SCANNER_SESSION_H_

#include <vector>

#include "ash/ash_export.h"

namespace ash {

class ScannerAction;

// A ScannerSession represents a single "use" of the Scanner feature. A session
// will be created when the feature is first triggered, until the feature is
// either dismissed, or commits its final result. The initialization of a
// session will be triggered on the creation of a new SunfishSession, however
// a ScannerSession's lifetime is not strictly bound to the lifetime of a
// SunfishSession.
class ASH_EXPORT ScannerSession {
 public:
  ScannerSession();
  ScannerSession(const ScannerSession&) = delete;
  ScannerSession& operator=(const ScannerSession&) = delete;
  ~ScannerSession();

  // Returns the actions that are currently available for this session.
  // TODO(b/363100868): Pass the required params here.
  std::vector<ScannerAction> ResolveActions();
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_SESSION_H_
