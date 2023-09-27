// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_chrome_feature_flags_instance.h"
#include "ash/components/arc/mojom/chrome_feature_flags.mojom.h"
#include "base/functional/callback_helpers.h"

namespace arc {

FakeChromeFeatureFlagsInstance::FakeChromeFeatureFlagsInstance() = default;

FakeChromeFeatureFlagsInstance::~FakeChromeFeatureFlagsInstance() = default;

void FakeChromeFeatureFlagsInstance::Init(
    mojo::PendingRemote<mojom::ChromeFeatureFlagsHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakeChromeFeatureFlagsInstance::NotifyFeatureFlags(
    mojom::FeatureFlagsPtr flags) {
  flags_called_value_ = std::move(flags);
}

}  // namespace arc
