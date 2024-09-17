// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <vector>

#include "ash/public/cpp/scanner/scanner_action.h"

namespace ash {

ScannerSession::ScannerSession() = default;

ScannerSession::~ScannerSession() = default;

std::vector<ScannerAction> ScannerSession::ResolveActions() {
  // TODO(b/363100868): Fetch actions available from the service.
  return {};
}

}  // namespace ash
