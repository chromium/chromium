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

void FakeCompatibilityModeInstance::IsOptimizedForCrosApp(
    const std::string& package_name,
    IsOptimizedForCrosAppCallback callback) {
  const bool is_o4c = o4c_pkgs_.find(package_name) != o4c_pkgs_.end();
  std::move(callback).Run(is_o4c);
}

}  // namespace arc
