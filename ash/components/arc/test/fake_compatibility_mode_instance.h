// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_COMPATIBILITY_MODE_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_COMPATIBILITY_MODE_INSTANCE_H_

#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "base/containers/flat_set.h"

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
  void IsOptimizedForCrosApp(const std::string& package_name,
                             IsOptimizedForCrosAppCallback callback) override;

  void set_o4c_pkg(std::string_view pkg_name) {
    o4c_pkgs_.emplace(std::string(pkg_name));
  }

 private:
  // Stores information for serving IsOptimizedForCrosApp calls.
  base::flat_set<std::string> o4c_pkgs_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_COMPATIBILITY_MODE_INSTANCE_H_
