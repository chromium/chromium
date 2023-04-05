// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_COMPATIBILITY_MODE_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_COMPATIBILITY_MODE_INSTANCE_H_

#include "ash/components/arc/mojom/compatibility_mode.mojom.h"

namespace arc {

class FakeCompatibilityModeInstance : public mojom::CompatibilityModeInstance {
 public:
  FakeCompatibilityModeInstance();
  FakeCompatibilityModeInstance(const FakeCompatibilityModeInstance&) = delete;
  FakeCompatibilityModeInstance& operator=(
      const FakeCompatibilityModeInstance&) = delete;
  ~FakeCompatibilityModeInstance() override;

  // mojom::CompatibilityModeInstance overrides:
  void SetResizeLockState(const std::string& package_name,
                          mojom::ArcResizeLockState state) override;
  void IsGioApplicable(const std::string& package_name,
                       IsGioApplicableCallback callback) override;

  void set_is_gio_applicable(bool is_gio_applicable) {
    is_gio_applicable_ = is_gio_applicable;
  }

 private:
  bool is_gio_applicable_ = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_COMPATIBILITY_MODE_INSTANCE_H_
