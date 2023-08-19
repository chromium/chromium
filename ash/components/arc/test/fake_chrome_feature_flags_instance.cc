// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_chrome_feature_flags_instance.h"
#include "base/functional/callback_helpers.h"

namespace arc {

FakeChromeFeatureFlagsInstance::FakeChromeFeatureFlagsInstance() = default;

FakeChromeFeatureFlagsInstance::~FakeChromeFeatureFlagsInstance() = default;

void FakeChromeFeatureFlagsInstance::NotifyFeatureFlags(
    mojom::FeatureFlagsPtr flags) {
  flags_called_value_ = std::move(flags);
}

}  // namespace arc
