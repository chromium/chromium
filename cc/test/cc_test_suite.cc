// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/cc_test_suite.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_id_name_manager.h"
#include "cc/base/histograms.h"
#include "components/viz/test/paths.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace cc {

namespace {
CCTestSuite* g_cc_test_suite = nullptr;
}  // namespace

CCTestSuite::CCTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {
  CHECK(!g_cc_test_suite);
  g_cc_test_suite = this;
}

CCTestSuite::~CCTestSuite() {
  CHECK_EQ(g_cc_test_suite, this);
  g_cc_test_suite = nullptr;
}

// static
void CCTestSuite::RunUntilIdle() {
  CHECK(g_cc_test_suite);
  CHECK(g_cc_test_suite->task_environment_);
  g_cc_test_suite->task_environment_->RunUntilIdle();
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
  task_environment_.reset();

  base::TestSuite::Shutdown();
}

}  // namespace cc
