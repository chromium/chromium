// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/unique_notifier.h"
#include "cc/paint/draw_image.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_raster_source.h"
#include "cc/tiles/tile_task_manager.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "url/gurl.h"

namespace cc {
namespace {

const size_t kMaxBytesPerCopyOperation = 1000U;
const size_t kMaxStagingBuffers = 32U;

enum RasterBufferProviderType {
  RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_GPU,
  RASTER_BUFFER_PROVIDER_TYPE_BITMAP
};

class TestRasterTaskCompletionHandler {
 public:
  virtual void OnRasterTaskCompleted(
      unsigned id,
      bool was_canceled) = 0;
};

class TestRasterTaskImpl : public TileTask {
 public:
  TestRasterTaskImpl(TestRasterTaskCompletionHandler* completion_handler,
                     unsigned id,
                     std::unique_ptr<RasterBuffer> raster_buffer,
                     TileTask::Vector* dependencies)
      : TileTask(true, dependencies),
        completion_handler_(completion_handler),
        id_(id),
        raster_buffer_(std::move(raster_buffer)),
        raster_source_(FakeRasterSource::CreateFilled(gfx::Size(1, 1))) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    RasterSource::PlaybackSettings settings;

    uint64_t new_content_id = 0;
    raster_buffer_->Playback(raster_source_.get(), gfx::Rect(1, 1),
                             gfx::Rect(1, 1), new_content_id,
                             gfx::AxisTransform2d(), settings, url_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    raster_buffer_ = nullptr;
    completion_handler_->OnRasterTaskCompleted(id_, state().IsCanceled());
  }

 protected:
  ~TestRasterTaskImpl() override = default;

 private:
  TestRasterTaskCompletionHandler* completion_handler_;
  unsigned id_;
  std::unique_ptr<RasterBuffer> raster_buffer_;
  scoped_refptr<RasterSource> raster_source_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterTaskImpl);
};

class BlockingTestRasterTaskImpl : public TestRasterTaskImpl {
 public:
  BlockingTestRasterTaskImpl(
      TestRasterTaskCompletionHandler* completion_handler,
      unsigned id,
      std::unique_ptr<RasterBuffer> raster_buffer,
      base::Lock* lock,
      TileTask::Vector* dependencies)
      : TestRasterTaskImpl(completion_handler,
                           id,
                           std::move(raster_buffer),
                           dependencies),
        lock_(lock) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    base::AutoLock lock(*lock_);
    TestRasterTaskImpl::RunOnWorkerThread();
  }

 protected:
  ~BlockingTestRasterTaskImpl() override = default;

 private:
  base::Lock* lock_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTestRasterTaskImpl);
};

class RasterBufferProviderTest
    : public TestRasterTaskCompletionHandler,
      public testing::TestWithParam<RasterBufferProviderType> {
 public:
  struct RasterTaskResult {
    unsigned id;
    bool canceled;
  };

  typedef std::vector<scoped_refptr<TileTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  RasterBufferProviderTest()
      : all_tile_tasks_finished_(
            base::ThreadTaskRunnerHandle::Get().get(),
            base::Bind(&RasterBufferProviderTest::AllTileTasksFinished,
                       base::Unretained(this))),
        timeout_seconds_(5),
        timed_out_(false) {}

  // Overridden from testing::Test:
  void SetUp() override {
    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        Create3dResourceProvider();
        raster_buffer_provider_ =
            std::make_unique<ZeroCopyRasterBufferProvider>(
                &gpu_memory_buffer_manager_, context_provider_.get(),
                viz::RGBA_8888);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        Create3dResourceProvider();
        raster_buffer_provider_ = std::make_unique<OneCopyRasterBufferProvider>(
            base::ThreadTaskRunnerHandle::Get().get(), context_provider_.get(),
            worker_context_provider_.get(), &gpu_memory_buffer_manager_,
            kMaxBytesPerCopyOperation, false, false, kMaxStagingBuffers,
            viz::RGBA_8888);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        Create3dResourceProvider();
        raster_buffer_provider_ = std::make_unique<GpuRasterBufferProvider>(
            context_provider_.get(), worker_context_provider_.get(), false, 0,
            viz::RGBA_8888, gfx::Size(), true, false, 1);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        CreateSoftwareResourceProvider();
        raster_buffer_provider_ = std::make_unique<BitmapRasterBufferProvider>(
            layer_tree_frame_sink_.get());
        break;
    }

    DCHECK(raster_buffer_provider_);

    pool_ = std::make_unique<ResourcePool>(
        resource_provider_.get(), context_provider_.get(),
        base::ThreadTaskRunnerHandle::Get(), base::TimeDelta(), true);
    tile_task_manager_ = TileTaskManagerImpl::Create(&task_graph_runner_);
  }

  void TearDown() override {
    for (auto& resource : resources_)
      pool_->ReleaseResource(std::move(resource));
    resources_.clear();
    tile_task_manager_->Shutdown();
    tile_task_manager_->CheckForCompletedTasks();

    raster_buffer_provider_->Shutdown();
    pool_.reset();
    resource_provider_.reset();
  }

  void AllTileTasksFinished() {
    tile_task_manager_->CheckForCompletedTasks();
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    task_graph_runner_.RunUntilIdle();
    tile_task_manager_->CheckForCompletedTasks();
  }

  void ScheduleTasks() {
    graph_.Reset();

    size_t priority = 0;

    for (RasterTaskVector::const_iterator it = tasks_.begin();
         it != tasks_.end(); ++it) {
      graph_.nodes.emplace_back(it->get(), 0 /* group */, priority++,
                                0 /* dependencies */);
    }

    tile_task_manager_->ScheduleTasks(&graph_);
  }

  ResourcePool::InUsePoolResource AllocateResource(const gfx::Size& size) {
    return pool_->AcquireResource(size, viz::RGBA_8888, gfx::ColorSpace());
  }

  void AppendTask(unsigned id, const gfx::Size& size) {
    ResourcePool::InUsePoolResource resource = AllocateResource(size);
    // The raster buffer has no tile ids associated with it for partial update,
    // so doesn't need to provide a valid dirty rect.
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(resource, 0, 0);
    TileTask::Vector empty;
    tasks_.push_back(
        new TestRasterTaskImpl(this, id, std::move(raster_buffer), &empty));
    resources_.push_back(std::move(resource));
  }

  void AppendTask(unsigned id) { AppendTask(id, gfx::Size(1, 1)); }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    ResourcePool::InUsePoolResource resource =
        AllocateResource(gfx::Size(1, 1));
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(resource, 0, 0);
    TileTask::Vector empty;
    tasks_.push_back(new BlockingTestRasterTaskImpl(
        this, id, std::move(raster_buffer), lock, &empty));
    resources_.push_back(std::move(resource));
  }

  void AppendTaskWithResource(unsigned id,
                              const ResourcePool::InUsePoolResource* resource) {
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(*resource, 0, 0);
    TileTask::Vector empty;
    tasks_.push_back(
        new TestRasterTaskImpl(this, id, std::move(raster_buffer), &empty));
  }

  const std::vector<RasterTaskResult>& completed_tasks() const {
    return completed_tasks_;
  }

  void LoseContext(viz::ContextProvider* context_provider) {
    if (!context_provider)
      return;
    context_provider->ContextGL()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    context_provider->ContextGL()->Flush();
  }

  void LoseContext(viz::RasterContextProvider* context_provider) {
    if (!context_provider)
      return;
    viz::RasterContextProvider::ScopedRasterContextLock lock(context_provider);
    context_provider->RasterInterface()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    context_provider->RasterInterface()->Flush();
  }

  void OnRasterTaskCompleted(unsigned id, bool was_canceled) override {
    RasterTaskResult result;
    result.id = id;
    result.canceled = was_canceled;
    completed_tasks_.push_back(result);
  }

 private:
  void Create3dResourceProvider() {
    auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
    gl_owned->set_support_sync_query(true);
    context_provider_ = viz::TestContextProvider::Create(std::move(gl_owned));
    context_provider_->BindToCurrentThread();
    worker_context_provider_ = viz::TestContextProvider::CreateWorker();
    layer_tree_frame_sink_ = FakeLayerTreeFrameSink::Create3d();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>(true);
  }

  void CreateSoftwareResourceProvider() {
    layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>(true);
  }

  void OnTimeout() {
    timed_out_ = true;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 protected:
  scoped_refptr<viz::TestContextProvider> context_provider_;
  scoped_refptr<viz::TestContextProvider> worker_context_provider_;
  std::unique_ptr<ResourcePool> pool_;
  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  std::unique_ptr<TileTaskManager> tile_task_manager_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  SynchronousTaskGraphRunner task_graph_runner_;
  base::CancelableClosure timeout_;
  UniqueNotifier all_tile_tasks_finished_;
  int timeout_seconds_;
  bool timed_out_;
  RasterTaskVector tasks_;
  std::vector<RasterTaskResult> completed_tasks_;
  std::vector<ResourcePool::InUsePoolResource> resources_;
  TaskGraph graph_;
};

TEST_P(RasterBufferProviderTest, Basic) {
  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterBufferProviderTest, FailedMapResource) {
  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_BITMAP)
    return;

  viz::TestGLES2Interface* gl = context_provider_->TestContextGL();
  gl->set_times_map_buffer_chromium_succeeds(0);
  AppendTask(0u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(1u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
}

// This test checks that replacing a pending raster task with another does
// not prevent the DidFinishRunningTileTasks notification from being sent.
TEST_P(RasterBufferProviderTest, FalseThrottling) {
  base::Lock lock;

  // Schedule a task that is prevented from completing with a lock.
  lock.Acquire();
  AppendBlockingTask(0u, &lock);
  ScheduleTasks();

  // Schedule another task to replace the still-pending task. Because the old
  // task is not a throttled task in the new task set, it should not prevent
  // DidFinishRunningTileTasks from getting signaled.
  RasterTaskVector tasks;
  tasks.swap(tasks_);
  AppendTask(1u);
  ScheduleTasks();

  // Unblock the first task to allow the second task to complete.
  lock.Release();

  RunMessageLoopUntilAllTasksHaveCompleted();
}

TEST_P(RasterBufferProviderTest, LostContext) {
  LoseContext(static_cast<viz::ContextProvider*>(context_provider_.get()));
  LoseContext(
      static_cast<viz::RasterContextProvider*>(worker_context_provider_.get()));

  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterBufferProviderTest, ReadyToDrawCallback) {
  AppendTask(0u);
  ScheduleTasks();
  RunMessageLoopUntilAllTasksHaveCompleted();

  std::vector<const ResourcePool::InUsePoolResource*> array;
  for (const auto& resource : resources_)
    array.push_back(&resource);

  base::RunLoop run_loop;
  uint64_t callback_id = raster_buffer_provider_->SetReadyToDrawCallback(
      array,
      base::Bind([](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop),
      0);

  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_GPU ||
      GetParam() == RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY)
    EXPECT_TRUE(callback_id);

  if (!callback_id)
    return;

  run_loop.Run();
}

TEST_P(RasterBufferProviderTest, ReadyToDrawCallbackNoDuplicate) {
  AppendTask(0u);
  ScheduleTasks();
  RunMessageLoopUntilAllTasksHaveCompleted();

  std::vector<const ResourcePool::InUsePoolResource*> array;
  for (const auto& resource : resources_)
    array.push_back(&resource);

  uint64_t callback_id = raster_buffer_provider_->SetReadyToDrawCallback(
      array, base::DoNothing(), 0);

  // Calling SetReadyToDrawCallback a second time for the same resources
  // should return the same callback ID.
  uint64_t callback_id_2 = raster_buffer_provider_->SetReadyToDrawCallback(
      array, base::DoNothing(), 0);

  EXPECT_EQ(callback_id, callback_id_2);

  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_GPU ||
      GetParam() == RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY)
    EXPECT_TRUE(callback_id);
}

TEST_P(RasterBufferProviderTest, WaitOnSyncTokenAfterReschedulingTask) {
  if (GetParam() != RASTER_BUFFER_PROVIDER_TYPE_GPU &&
      GetParam() != RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY)
    return;

  base::Lock lock;

  // Schedule a task that is prevented from completing with a lock.
  lock.Acquire();
  AppendBlockingTask(0u, &lock);
  ScheduleTasks();

  EXPECT_EQ(resources_.size(), 1u);
  const ResourcePool::InUsePoolResource* resource = &resources_[0];

  // Schedule another task to replace the still-pending task using the same
  // resource.
  RasterTaskVector tasks;
  tasks.swap(tasks_);
  AppendTaskWithResource(1u, resource);
  ScheduleTasks();

  // The first task is canceled, but the second task uses the same resource, and
  // waits on the compositor sync token that was left by the first task.
  RunMessageLoopUntilAllTasksHaveCompleted();

  {
    viz::ContextProvider::ScopedContextLock context_lock(
        worker_context_provider_.get());
    viz::TestGLES2Interface* gl = worker_context_provider_->TestContextGL();
    EXPECT_TRUE(gl->last_waited_sync_token().HasData());
  }

  lock.Release();

  ASSERT_EQ(completed_tasks().size(), 2u);
  EXPECT_TRUE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterBufferProviderTest, MeasureGpuRasterDuration) {
  if (GetParam() != RASTER_BUFFER_PROVIDER_TYPE_GPU)
    return;

  // Schedule a task.
  AppendTask(0u);
  ScheduleTasks();
  RunMessageLoopUntilAllTasksHaveCompleted();

  // Wait for the GPU side work to finish.
  base::RunLoop run_loop;
  std::vector<const ResourcePool::InUsePoolResource*> array;
  for (const auto& resource : resources_)
    array.push_back(&resource);
  uint64_t callback_id = raster_buffer_provider_->SetReadyToDrawCallback(
      array,
      base::Bind([](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop),
      0);
  ASSERT_TRUE(callback_id);
  run_loop.Run();

  // Poll the task and make sure a histogram is logged.
  base::HistogramTester histogram_tester;
  std::string histogram("Renderer4.Renderer.RasterTaskTotalDuration.Gpu");
  histogram_tester.ExpectTotalCount(histogram, 0);
  bool has_pending_queries =
      raster_buffer_provider_->CheckRasterFinishedQueries();
  EXPECT_FALSE(has_pending_queries);
  histogram_tester.ExpectTotalCount(histogram, 1);
}

INSTANTIATE_TEST_CASE_P(
    RasterBufferProviderTests,
    RasterBufferProviderTest,
    ::testing::Values(RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
                      RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
                      RASTER_BUFFER_PROVIDER_TYPE_GPU,
                      RASTER_BUFFER_PROVIDER_TYPE_BITMAP));

}  // namespace
}  // namespace cc
