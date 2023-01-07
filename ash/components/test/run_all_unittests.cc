// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/test/ash_components_test_suite.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

int main(int argc, char** argv) {
  ash::AshComponentsTestSuite test_suite(argc, argv);

  mojo::core::Init();
  // The IPC thread is necessary for the window service.
  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&ash::AshComponentsTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
