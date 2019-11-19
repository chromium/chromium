// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/cc_test_suite.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_id_name_manager.h"
#include "cc/base/histograms.h"
#include "components/viz/test/paths.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace cc {

CCTestSuite::CCTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

CCTestSuite::~CCTestSuite() = default;

void CCTestSuite::Initialize() {
  base::TestSuite::Initialize();
  message_loop_ = std::make_unique<base::MessageLoop>();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  viz::Paths::RegisterPathProvider();

  base::ThreadIdNameManager::GetInstance()->SetName("Main");

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  SetClientNameForMetrics("Renderer");
}

void CCTestSuite::Shutdown() {
  message_loop_ = nullptr;

  base::TestSuite::Shutdown();
}

}  // namespace cc
