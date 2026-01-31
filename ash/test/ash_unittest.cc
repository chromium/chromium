// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "mojo/core/embedder/embedder.h"

namespace ash {

// A significant number of ash_unittests are overriding the feature list after
// the GPU thread is started so allowlist the whole test binary.
// TODO(crbug.com/40785850): Fix racy tests and remove this.
class AshScopedAllowRacyFeatureListOverrides {
 private:
  viz::TestGpuServiceHolder::ScopedAllowRacyFeatureListOverrides
      gpu_thread_allow_racy_overrides_;
};

}  // namespace ash

int main(int argc, char** argv) {
  ash::AshScopedAllowRacyFeatureListOverrides gpu_thread_allow_racy_overrides;
  ash::AshTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ash::AshTestSuite::Run, base::Unretained(&test_suite)));
}
