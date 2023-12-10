// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_CHROME_FEATURE_FLAGS_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_CHROME_FEATURE_FLAGS_INSTANCE_H_

#include "ash/components/arc/mojom/chrome_feature_flags.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {

class FakeChromeFeatureFlagsInstance
    : public mojom::ChromeFeatureFlagsInstance {
 public:
  FakeChromeFeatureFlagsInstance();

  FakeChromeFeatureFlagsInstance(const FakeChromeFeatureFlagsInstance&) =
      delete;
  FakeChromeFeatureFlagsInstance& operator=(
      const FakeChromeFeatureFlagsInstance&) = delete;

  ~FakeChromeFeatureFlagsInstance() override;

  const mojom::FeatureFlagsPtr& flags_called_value() {
    return flags_called_value_.value();
  }

  // mojom::ChromeFeatureFlagsInstance overrides:
  void NotifyFeatureFlags(mojom::FeatureFlagsPtr flags) override;

 private:
  std::optional<mojom::FeatureFlagsPtr> flags_called_value_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_CHROME_FEATURE_FLAGS_INSTANCE_H_
