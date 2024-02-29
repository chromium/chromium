// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/mock_scanning_enabled_provider.h"

namespace ash::quick_pair {

MockScanningEnabledProvider::MockScanningEnabledProvider()
    : ScanningEnabledProvider(nullptr, nullptr, nullptr, nullptr) {}

MockScanningEnabledProvider::~MockScanningEnabledProvider() = default;

}  // namespace ash::quick_pair
