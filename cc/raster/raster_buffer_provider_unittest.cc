// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/unique_notifier.h"
#include "cc/paint/draw_image.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_query_queue.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_raster_source.h"
#include "cc/tiles/tile_task_manager.h"
#include "cc/trees/raster_capabilities.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
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
      : TileTask(TileTask::SupportsConcurrentExecution::kYes,
                 TileTask::SupportsBackgroundThreadPriority::kYes,
                 dependencies),
        completion_handler_(completion_handler),
        id_(id),
        raster_buffer_(std::move(raster_buffer)),
        raster_source_(FakeRasterSource::CreateFilled(gfx::Size(1, 1))) {}
  TestRasterTaskImpl(const TestRasterTaskImpl&) = delete;
  TestRasterTaskImpl& operator=(const TestRasterTaskImpl&) = delete;

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
  raw_ptr<TestRasterTaskCompletionHandler> completion_handler_;
  unsigned id_;
  std::unique_ptr<RasterBuffer> raster_buffer_;
  scoped_refptr<RasterSource> raster_source_;
  GURL url_;
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
  BlockingTestRasterTaskImpl(const BlockingTestRasterTaskImpl&) = delete;
  BlockingTestRasterTaskImpl& operator=(const BlockingTestRasterTaskImpl&) =
      delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {
    base::AutoLock lock(*lock_);
    TestRasterTaskImpl::RunOnWorkerThread();
  }

 protected:
  ~BlockingTestRasterTaskImpl() override = default;

 private:
  raw_ptr<base::Lock> lock_;
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
            base::SingleThreadTaskRunner::GetCurrentDefault().get(),
            base::BindRepeating(&RasterBufferProviderTest::AllTileTasksFinished,
                                base::Unretained(this))),
        timeout_seconds_(5),
        timed_out_(false) {}

  // Overridden from testing::Test:
  void SetUp() override {
    RasterCapabilities raster_caps;
    raster_caps.tile_format = viz::SinglePlaneFormat::kRGBA_8888;

    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        Create3dResourceProvider();
        raster_caps.use_gpu_rasterization = false;
        raster_buffer_provider_ =
            std::make_unique<ZeroCopyRasterBufferProvider>(
                context_provider_.get(), raster_caps);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        Create3dResourceProvider();
        raster_caps.use_gpu_rasterization = false;
        raster_buffer_provider_ = std::make_unique<OneCopyRasterBufferProvider>(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(),
            context_provider_.get(), worker_context_provider_.get(),
            kMaxBytesPerCopyOperation, false, kMaxStagingBuffers, raster_caps);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        Create3dResourceProvider();
        raster_caps.use_gpu_rasterization = true;
        raster_buffer_provider_ = std::make_unique<GpuRasterBufferProvider>(
            context_provider_.get(), worker_context_provider_.get(),
            raster_caps, gfx::Size(), true, pending_raster_queries_.get(), 1);
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
        base::SingleThreadTaskRunner::GetCurrentDefault(), base::TimeDelta(),
        true);
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
    return pool_->AcquireResource(size, viz::SinglePlaneFormat::kRGBA_8888,
                                  gfx::ColorSpace());
  }

  void AppendTask(unsigned id,
                  const gfx::Size& size,
                  bool depends_on_at_raster_decodes,
                  bool depends_on_hardware_accelerated_jpeg_candidates,
                  bool depends_on_hardware_accelerated_webp_candidates) {
    ResourcePool::InUsePoolResource resource = AllocateResource(size);
    // The raster buffer has no tile ids associated with it for partial update,
    // so doesn't need to provide a valid dirty rect.
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(
            resource, 0, 0, depends_on_at_raster_decodes,
            depends_on_hardware_accelerated_jpeg_candidates,
            depends_on_hardware_accelerated_webp_candidates);
    TileTask::Vector empty;
    tasks_.push_back(
        new TestRasterTaskImpl(this, id, std::move(raster_buffer), &empty));
    resources_.push_back(std::move(resource));
  }

  void AppendTask(unsigned id) {
    AppendTask(id, gfx::Size(1, 1), false /* depends_on_at_raster_decodes */,
               false /* depends_on_hardware_accelerated_jpeg_candidates */,
               false /* depends_on_hardware_accelerated_webp_candidates */);
  }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    ResourcePool::InUsePoolResource resource =
        AllocateResource(gfx::Size(1, 1));
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(
            resource, 0, 0, false /* depends_on_at_raster_decodes */,
            false /* depends_on_hardware_accelerated_jpeg_candidates */,
            false /* depends_on_hardware_accelerated_webp_candidates */);
    TileTask::Vector empty;
    tasks_.push_back(new BlockingTestRasterTaskImpl(
        this, id, std::move(raster_buffer), lock, &empty));
    resources_.push_back(std::move(resource));
  }

  void AppendTaskWithResource(unsigned id,
                              const ResourcePool::InUsePoolResource* resource) {
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(
            *resource, 0, 0, false /* depends_on_at_raster_decodes */,
            false /* depends_on_hardware_accelerated_jpeg_candidates */,
            false /* depends_on_hardware_accelerated_webp_candidates */);
    TileTask::Vector empty;
    tasks_.push_back(
        new TestRasterTaskImpl(this, id, std::move(raster_buffer), &empty));
  }

  const std::vector<RasterTaskResult>& completed_tasks() const {
    return completed_tasks_;
  }

  void LoseContext(viz::RasterContextProvider* context_provider,
                   bool use_lock) {
    if (!context_provider) {
      return;
    }

    std::optional<viz::RasterContextProvider::ScopedRasterContextLock> lock;
    if (use_lock) {
      lock.emplace(context_provider);
    }

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
    context_provider_ = viz::TestContextProvider::Create();
    context_provider_->BindToCurrentSequence();

    worker_context_provider_ = viz::TestContextProvider::CreateWorker();
    DCHECK(worker_context_provider_);

    layer_tree_frame_sink_ = FakeLayerTreeFrameSink::Create3d(
        context_provider_, worker_context_provider_);
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();

    pending_raster_queries_ =
        std::make_unique<RasterQueryQueue>(worker_context_provider_.get());
  }

  void CreateSoftwareResourceProvider() {
    layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
  }

  void OnTimeout() {
    timed_out_ = true;
  }

 protected:
  scoped_refptr<viz::TestContextProvider> context_provider_;
  scoped_refptr<viz::TestContextProvider> worker_context_provider_;
  std::unique_ptr<ResourcePool> pool_;
  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  std::unique_ptr<TileTaskManager> tile_task_manager_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  SynchronousTaskGraphRunner task_graph_runner_;
  UniqueNotifier all_tile_tasks_finished_;
  int timeout_seconds_;
  bool timed_out_;
  RasterTaskVector tasks_;
  std::vector<RasterTaskResult> completed_tasks_;
  std::vector<ResourcePool::InUsePoolResource> resources_;
  TaskGraph graph_;
  std::unique_ptr<RasterQueryQueue> pending_raster_queries_;
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
  LoseContext(context_provider_.get(), /*use_lock=*/false);
  LoseContext(worker_context_provider_.get(), /*use_lock=*/true);

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
      array, run_loop.QuitClosure(), 0);

  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_GPU ||
      GetParam() == RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY) {
    EXPECT_TRUE(callback_id);
  }

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
      GetParam() == RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY) {
    EXPECT_TRUE(callback_id);
  }
}

TEST_P(RasterBufferProviderTest, WaitOnSyncTokenAfterReschedulingTask) {
  if (GetParam() != RASTER_BUFFER_PROVIDER_TYPE_GPU &&
      GetParam() != RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY) {
    return;
  }

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
    viz::RasterContextProvider::ScopedRasterContextLock context_lock(
        worker_context_provider_.get());
    viz::TestRasterInterface* ri =
        worker_context_provider_->GetTestRasterInterface();
    EXPECT_TRUE(ri->last_waited_sync_token().HasData());
  }

  lock.Release();

  ASSERT_EQ(completed_tasks().size(), 2u);
  EXPECT_TRUE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterBufferProviderTest, MeasureGpuRasterDuration) {
  if (GetParam() != RASTER_BUFFER_PROVIDER_TYPE_GPU) {
    return;
  }

  // Schedule a few tasks.
  constexpr gfx::Size size(1, 1);
  AppendTask(0u, size, false /* depends_on_at_raster_decodes */,
             false /* depends_on_hardware_accelerated_jpeg_candidates */,
             false /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(1u, size, false /* depends_on_at_raster_decodes */,
             false /* depends_on_hardware_accelerated_jpeg_candidates */,
             true /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(2u, size, false /* depends_on_at_raster_decodes */,
             true /* depends_on_hardware_accelerated_jpeg_candidates */,
             false /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(3u, size, false /* depends_on_at_raster_decodes */,
             true /* depends_on_hardware_accelerated_jpeg_candidates */,
             false /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(4u, size, false /* depends_on_at_raster_decodes */,
             true /* depends_on_hardware_accelerated_jpeg_candidates */,
             true /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(5u, size, true /* depends_on_at_raster_decodes */,
             false /* depends_on_hardware_accelerated_jpeg_candidates */,
             false /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(6u, size, true /* depends_on_at_raster_decodes */,
             false /* depends_on_hardware_accelerated_jpeg_candidates */,
             true /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(7u, size, true /* depends_on_at_raster_decodes */,
             true /* depends_on_hardware_accelerated_jpeg_candidates */,
             false /* depends_on_hardware_accelerated_webp_candidates */);
  AppendTask(8u, size, true /* depends_on_at_raster_decodes */,
             true /* depends_on_hardware_accelerated_jpeg_candidates */,
             true /* depends_on_hardware_accelerated_webp_candidates */);
  ScheduleTasks();
  RunMessageLoopUntilAllTasksHaveCompleted();

  // Wait for the GPU side work to finish.
  base::RunLoop run_loop;
  std::vector<const ResourcePool::InUsePoolResource*> array;
  for (const auto& resource : resources_)
    array.push_back(&resource);
  uint64_t callback_id = raster_buffer_provider_->SetReadyToDrawCallback(
      array, run_loop.QuitClosure(), 0);
  ASSERT_TRUE(callback_id);
  run_loop.Run();

  // Poll the task and make sure histograms are logged.
  base::HistogramTester histogram_tester;
  std::string duration_histogram(
      "Renderer4.Renderer.RasterTaskTotalDuration.Oop");
  std::string delay_histogram_all_tiles(
      "Renderer4.Renderer.RasterTaskSchedulingDelayNoAtRasterDecodes.All");
  std::string delay_histogram_jpeg_tiles(
      "Renderer4.Renderer.RasterTaskSchedulingDelayNoAtRasterDecodes."
      "TilesWithJpegHwDecodeCandidates");
  std::string delay_histogram_webp_tiles(
      "Renderer4.Renderer.RasterTaskSchedulingDelayNoAtRasterDecodes."
      "TilesWithWebPHwDecodeCandidates");
  histogram_tester.ExpectTotalCount(duration_histogram, 0);
  histogram_tester.ExpectTotalCount(delay_histogram_all_tiles, 0);
  histogram_tester.ExpectTotalCount(delay_histogram_jpeg_tiles, 0);
  histogram_tester.ExpectTotalCount(delay_histogram_webp_tiles, 0);
  bool has_pending_queries =
      pending_raster_queries_->CheckRasterFinishedQueries();
  EXPECT_FALSE(has_pending_queries);
  histogram_tester.ExpectTotalCount(duration_histogram, 9);

  // Only in Chrome OS, we should be measuring raster scheduling delay (and only
  // for tasks that don't depend on at-raster image decodes).
  base::HistogramBase::Count expected_delay_histogram_all_tiles_count = 0;
  base::HistogramBase::Count expected_delay_histogram_jpeg_tiles_count = 0;
  base::HistogramBase::Count expected_delay_histogram_webp_tiles_count = 0;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_GPU) {
    expected_delay_histogram_all_tiles_count = 5;
    expected_delay_histogram_jpeg_tiles_count = 3;
    expected_delay_histogram_webp_tiles_count = 2;
  }
#endif
  histogram_tester.ExpectTotalCount(delay_histogram_all_tiles,
                                    expected_delay_histogram_all_tiles_count);
  histogram_tester.ExpectTotalCount(delay_histogram_jpeg_tiles,
                                    expected_delay_histogram_jpeg_tiles_count);
  histogram_tester.ExpectTotalCount(delay_histogram_webp_tiles,
                                    expected_delay_histogram_webp_tiles_count);
}

INSTANTIATE_TEST_SUITE_P(
    RasterBufferProviderTests,
    RasterBufferProviderTest,
    ::testing::Values(RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
                      RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
                      RASTER_BUFFER_PROVIDER_TYPE_GPU,
                      RASTER_BUFFER_PROVIDER_TYPE_BITMAP));

}  // namespace
}  // namespace cc
