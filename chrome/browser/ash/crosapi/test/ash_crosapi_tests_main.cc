// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/test_launcher.h"
#include "base/threading/thread.h"
#include "chrome/browser/ash/crosapi/test/ash_crosapi_tests_env.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Starts a new IO thread to run IPC tasks.
  base::Thread io_thread("MojoThread");
  io_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  // Sets up crosapi test environment.
  auto env = std::make_unique<crosapi::AshCrosapiTestEnv>();

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
