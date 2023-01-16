// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace {

// Test Suite used for Crosapi Test.
// It initializes Mojo and AshCrosapiTestEnv.
class CrosapiTestSuite : public base::TestSuite {
 public:
  CrosapiTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  CrosapiTestSuite(const CrosapiTestSuite&) = delete;
  CrosapiTestSuite& operator=(const CrosapiTestSuite&) = delete;

  ~CrosapiTestSuite() override = default;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

    // Starts a new IO thread to run IPC tasks.
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    mojo::core::Init();
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        io_thread_.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    executor_ = std::make_unique<base::SingleThreadTaskExecutor>(
        base::MessagePumpType::IO);
  }

  void Shutdown() override {
    executor_.reset();
    ipc_support_.reset();
    io_thread_.Stop();
    base::TestSuite::Shutdown();
  }

 private:
  base::Thread io_thread_{"MojoThread"};
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  std::unique_ptr<base::SingleThreadTaskExecutor> executor_;
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  CrosapiTestSuite test_suite(argc, argv);
  // Run test serially.
  // TODO(elkurin): Support parallel testing for better performance.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
