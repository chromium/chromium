// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/threading/thread.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace ash {

// A significant number of ash_unittests are overriding the feature list after
// the GPU thread is started so allowlist the whole test binary.
// TODO(crbug.com/1241161): Fix racy tests and remove this.
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
  // The IPC thread is necessary for the window service.
  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ash::AshTestSuite::Run, base::Unretained(&test_suite)));
}
