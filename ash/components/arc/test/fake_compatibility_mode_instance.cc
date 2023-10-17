// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_compatibility_mode_instance.h"

namespace arc {

FakeCompatibilityModeInstance::FakeCompatibilityModeInstance() = default;

FakeCompatibilityModeInstance::~FakeCompatibilityModeInstance() = default;

void FakeCompatibilityModeInstance::SetResizeLockState(
    const std::string& package_name,
    mojom::ArcResizeLockState state) {}

void FakeCompatibilityModeInstance::IsGioApplicable(
    const std::string& package_name,
    IsGioApplicableCallback callback) {
  std::move(callback).Run(is_gio_applicable_);
}

}  // namespace arc
