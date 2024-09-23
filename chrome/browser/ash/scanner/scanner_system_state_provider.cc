// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_system_state_provider.h"

#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"

ScannerSystemStateProvider::ScannerSystemStateProvider() = default;

ScannerSystemStateProvider::~ScannerSystemStateProvider() = default;

ash::ScannerSystemState ScannerSystemStateProvider::GetSystemState() const {
  // TODO(b/363100868): Add required system checks for the feature.
  return ash::ScannerSystemState(ash::ScannerStatus::kBlocked, {});
}
