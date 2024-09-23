// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/scanner/scanner_action.h"

#include <string_view>

namespace ash {

ScannerAction::ScannerAction(std::string_view display_name,
                             const ScannerCommand& command)
    : display_name(std::string(display_name)), command(command) {}

ScannerAction::ScannerAction(const ScannerAction& rhs) = default;

ScannerAction::~ScannerAction() = default;

}  // namespace ash
