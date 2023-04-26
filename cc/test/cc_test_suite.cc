// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/cc_test_suite.h"

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_id_name_manager.h"
#include "cc/base/histograms.h"
#include "components/viz/test/paths.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace cc {

std::unique_ptr<base::test::TaskEnvironment> CCTestSuite::task_environment_;

CCTestSuite::CCTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

CCTestSuite::~CCTestSuite() = default;

// static
void CCTestSuite::RunUntilIdle() {
  CHECK(task_environment_);
  task_environment_->RunUntilIdle();
}

void CCTestSuite::Initialize() {
  base::TestSuite::Initialize();

  CHECK(!task_environment_);
  task_environment_ = std::make_unique<base::test::TaskEnvironment>();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  viz::Paths::RegisterPathProvider();

  base::ThreadIdNameManager::GetInstance()->SetName("Main");

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  SetClientNameForMetrics("Renderer");
}

void CCTestSuite::Shutdown() {
  CHECK(task_environment_);
  task_environment_ = nullptr;

  base::TestSuite::Shutdown();
}

}  // namespace cc
