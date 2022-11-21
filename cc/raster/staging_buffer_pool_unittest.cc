// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/staging_buffer_pool.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(StagingBufferPoolTest, ShutdownImmediatelyAfterCreation) {
  auto context_provider = viz::TestContextProvider::CreateWorker();
  bool use_partial_raster = false;
  int max_staging_buffer_usage_in_bytes = 1024;
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  // Create a StagingBufferPool and immediately shut it down.
  auto pool = std::make_unique<StagingBufferPool>(
      task_runner.get(), context_provider.get(), use_partial_raster,
      max_staging_buffer_usage_in_bytes);
  pool->Shutdown();
  // Flush the message loop.
  auto flush_message_loop = [] {
    base::RunLoop runloop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, runloop.QuitClosure());
    runloop.Run();
  };

  // Constructing the pool does a post-task to add itself as an observer. So
  // allow for that registration to complete first.
  flush_message_loop();

  // Now, destroy the pool, and trigger a notification from the
  // MemoryPressureListener.
  pool = nullptr;
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // Allow the callbacks in the observers to run.
  flush_message_loop();
  // No crash.
}

}  // namespace cc
