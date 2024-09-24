// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "build/build_config.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_query_queue.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/tiles/tile_task_manager.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace cc {
namespace {

class PerfGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
  // Overridden from gpu::gles2::GLES2Interface:
  void GenBuffers(GLsizei n, GLuint* buffers) override {
    for (GLsizei i = 0; i < n; ++i)
      buffers[i] = 1u;
  }
  void GenTextures(GLsizei n, GLuint* textures) override {
    for (GLsizei i = 0; i < n; ++i)
      textures[i] = 1u;
  }
  void GetIntegerv(GLenum pname, GLint* params) override {
    if (pname == GL_MAX_TEXTURE_SIZE)
      *params = INT_MAX;
  }
  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    for (GLsizei i = 0; i < n; ++i)
      queries[i] = 1u;
  }
  void GetQueryObjectuivEXT(GLuint query,
                            GLenum pname,
                            GLuint* params) override {
    if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
      *params = 1;
  }

  // Overridden from gpu::InterfaceBase
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override {
    // Copy the data over after setting the data to ensure alignment.
    gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId(), 0);
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }
};

}  // namespace

class PerfContextProvider
    : public base::RefCountedThreadSafe<PerfContextProvider>,
      public viz::RasterContextProvider {
 public:
  PerfContextProvider()
      : context_gl_(new PerfGLES2Interface),
        cache_controller_(&support_, nullptr) {
    capabilities_.sync_query = true;

    raster_context_ = std::make_unique<gpu::raster::RasterImplementationGLES>(
        context_gl_.get(), ContextSupport(), capabilities_);
  }

  // viz::RasterContextProvider implementation.
  void AddRef() const override {
    base::RefCountedThreadSafe<PerfContextProvider>::AddRef();
  }
  void Release() const override {
    base::RefCountedThreadSafe<PerfContextProvider>::Release();
  }

  gpu::ContextResult BindToCurrentSequence() override {
    return gpu::ContextResult::kSuccess;
  }
  const gpu::Capabilities& ContextCapabilities() const override {
    return capabilities_;
  }
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    return gpu_feature_info_;
  }
  gpu::raster::RasterInterface* RasterInterface() override {
    return raster_context_.get();
  }
  gpu::ContextSupport* ContextSupport() override { return &support_; }
  class GrDirectContext* GrContext() override {
    if (!test_context_provider_) {
      test_context_provider_ = viz::TestContextProvider::CreateRaster();
    }
    return test_context_provider_->GrContext();
  }
  gpu::SharedImageInterface* SharedImageInterface() override {
    if (!test_context_provider_) {
      test_context_provider_ = viz::TestContextProvider::CreateRaster();
    }
    return test_context_provider_->SharedImageInterface();
  }
  viz::ContextCacheController* CacheController() override {
    return &cache_controller_;
  }
  base::Lock* GetLock() override { return &context_lock_; }
  void AddObserver(viz::ContextLostObserver* obs) override {}
  void RemoveObserver(viz::ContextLostObserver* obs) override {}
  unsigned int GetGrGLTextureFormat(
      viz::SharedImageFormat format) const override {
    return viz::SharedImageFormatRestrictedSinglePlaneUtils::
        ToGLTextureStorageFormat(
            format, ContextCapabilities().angle_rgbx_internal_format);
  }

 private:
  friend class base::RefCountedThreadSafe<PerfContextProvider>;

  ~PerfContextProvider() override = default;

  std::unique_ptr<PerfGLES2Interface> context_gl_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_context_;

  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  viz::TestContextSupport support_;
  viz::ContextCacheController cache_controller_;
  base::Lock context_lock_;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
};

namespace {

enum RasterBufferProviderType {
  RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_GPU,
  RASTER_BUFFER_PROVIDER_TYPE_BITMAP
};

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfTileTask : public TileTask {
 public:
  explicit PerfTileTask(TileTask::Vector* dependencies = nullptr)
      : TileTask(TileTask::SupportsConcurrentExecution::kYes,
                 TileTask::SupportsBackgroundThreadPriority::kYes,
                 dependencies) {}

  void Reset() {
    did_complete_ = false;
    state().Reset();
  }

  void Cancel() {
    if (!state().IsCanceled())
      state().DidCancel();

    did_complete_ = true;
  }

 protected:
  ~PerfTileTask() override = default;
};

class PerfImageDecodeTaskImpl : public PerfTileTask {
 public:
  PerfImageDecodeTaskImpl() = default;
  PerfImageDecodeTaskImpl(const PerfImageDecodeTaskImpl&) = delete;

  PerfImageDecodeTaskImpl& operator=(const PerfImageDecodeTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  // Overridden from TileTask:
  void OnTaskCompleted() override {}

 protected:
  ~PerfImageDecodeTaskImpl() override = default;
};

class PerfRasterBufferProviderHelper {
 public:
  virtual std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes) = 0;
};

class PerfRasterTaskImpl : public PerfTileTask {
 public:
  PerfRasterTaskImpl(ResourcePool* pool,
                     ResourcePool::InUsePoolResource in_use_resource,
                     std::unique_ptr<RasterBuffer> raster_buffer,
                     TileTask::Vector* dependencies)
      : PerfTileTask(dependencies),
        pool_(pool),
        resource_(std::move(in_use_resource)),
        raster_buffer_(std::move(raster_buffer)) {}
  PerfRasterTaskImpl(const PerfRasterTaskImpl&) = delete;
  PerfRasterTaskImpl& operator=(const PerfRasterTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    // Note: Perf tests will Reset() the PerfTileTask, causing it to be
    // completed multiple times. We can only do the work of completion once
    // though.
    if (raster_buffer_) {
      raster_buffer_ = nullptr;
      pool_->ReleaseResource(std::move(resource_));
    }
  }

 protected:
  ~PerfRasterTaskImpl() override = default;

 private:
  const raw_ptr<ResourcePool> pool_;
  ResourcePool::InUsePoolResource resource_;
  std::unique_ptr<RasterBuffer> raster_buffer_;
};

class RasterBufferProviderPerfTestBase {
 public:
  typedef std::vector<scoped_refptr<TileTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  RasterBufferProviderPerfTestBase()
      : compositor_context_provider_(
            base::MakeRefCounted<PerfContextProvider>()),
        worker_context_provider_(base::MakeRefCounted<PerfContextProvider>()),
        task_runner_(new base::TestSimpleTaskRunner),
        task_graph_runner_(new SynchronousTaskGraphRunner),
        timer_(kWarmupRuns,
               base::Milliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void CreateImageDecodeTasks(unsigned num_image_decode_tasks,
                              TileTask::Vector* image_decode_tasks) {
    for (unsigned i = 0; i < num_image_decode_tasks; ++i)
      image_decode_tasks->push_back(new PerfImageDecodeTaskImpl);
  }

  void CreateRasterTasks(PerfRasterBufferProviderHelper* helper,
                         unsigned num_raster_tasks,
                         const TileTask::Vector& image_decode_tasks,
                         RasterTaskVector* raster_tasks) {
    const gfx::Size size(1, 1);

    for (unsigned i = 0; i < num_raster_tasks; ++i) {
      ResourcePool::InUsePoolResource in_use_resource =
          resource_pool_->AcquireResource(
              size, viz::SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace());

      // No tile ids are given to support partial updates.
      std::unique_ptr<RasterBuffer> raster_buffer;
      if (helper)
        raster_buffer =
            helper->AcquireBufferForRaster(in_use_resource, 0, 0, false);
      TileTask::Vector dependencies = image_decode_tasks;
      raster_tasks->push_back(new PerfRasterTaskImpl(
          resource_pool_.get(), std::move(in_use_resource),
          std::move(raster_buffer), &dependencies));
    }
  }

  void ResetRasterTasks(const RasterTaskVector& raster_tasks) {
    for (auto& raster_task : raster_tasks) {
      for (auto& decode_task : raster_task->dependencies())
        static_cast<PerfTileTask*>(decode_task.get())->Reset();

      static_cast<PerfTileTask*>(raster_task.get())->Reset();
    }
  }

  void CancelRasterTasks(const RasterTaskVector& raster_tasks) {
    for (auto& raster_task : raster_tasks) {
      for (auto& decode_task : raster_task->dependencies())
        static_cast<PerfTileTask*>(decode_task.get())->Cancel();

      static_cast<PerfTileTask*>(raster_task.get())->Cancel();
    }
  }

  void BuildTileTaskGraph(TaskGraph* graph,
                          const RasterTaskVector& raster_tasks) {
    uint16_t priority = 0;

    for (auto& raster_task : raster_tasks) {
      priority++;

      for (auto& decode_task : raster_task->dependencies()) {
        // Add decode task if it doesn't already exist in graph.
        if (!base::Contains(graph->nodes, decode_task,
                            &TaskGraph::Node::task)) {
          graph->nodes.push_back(
              TaskGraph::Node(decode_task.get(), 0u /* group */, priority, 0u));
        }

        graph->edges.push_back(
            TaskGraph::Edge(decode_task.get(), raster_task.get()));
      }

      graph->nodes.push_back(TaskGraph::Node(
          raster_task.get(), 0u /* group */, priority,
          static_cast<uint32_t>(raster_task->dependencies().size())));
    }
  }

 protected:
  scoped_refptr<viz::RasterContextProvider> compositor_context_provider_;
  scoped_refptr<viz::RasterContextProvider> worker_context_provider_;
  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<ResourcePool> resource_pool_;
  std::unique_ptr<SynchronousTaskGraphRunner> task_graph_runner_;
  base::LapTimer timer_;
};

class RasterBufferProviderPerfTest
    : public RasterBufferProviderPerfTestBase,
      public PerfRasterBufferProviderHelper,
      public testing::TestWithParam<RasterBufferProviderType> {
 public:
  // Overridden from testing::Test:
  void SetUp() override {
    pending_raster_queries_ =
        std::make_unique<RasterQueryQueue>(worker_context_provider_.get());

    RasterCapabilities raster_caps;
    raster_caps.tile_format = viz::SinglePlaneFormat::kRGBA_8888;

    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        Create3dResourceProvider();
        raster_caps.use_gpu_rasterization = false;
        raster_buffer_provider_ =
            std::make_unique<ZeroCopyRasterBufferProvider>(
                compositor_context_provider_.get(), raster_caps);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        Create3dResourceProvider();
        raster_caps.use_gpu_rasterization = false;
        raster_buffer_provider_ = std::make_unique<OneCopyRasterBufferProvider>(
            task_runner_.get(), compositor_context_provider_.get(),
            worker_context_provider_.get(), std::numeric_limits<int>::max(),
            false, std::numeric_limits<int>::max(), raster_caps);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        Create3dResourceProvider();
        raster_caps.use_gpu_rasterization = true;
        raster_buffer_provider_ = std::make_unique<GpuRasterBufferProvider>(
            compositor_context_provider_.get(), worker_context_provider_.get(),
            raster_caps, gfx::Size(), true, pending_raster_queries_.get());
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        CreateSoftwareResourceProvider();
        raster_buffer_provider_ = std::make_unique<BitmapRasterBufferProvider>(
            layer_tree_frame_sink_.get());
        break;
    }
    DCHECK(raster_buffer_provider_);

    resource_pool_ = std::make_unique<ResourcePool>(
        resource_provider_.get(), compositor_context_provider_.get(),
        task_runner_, ResourcePool::kDefaultExpirationDelay, false);
    tile_task_manager_ = TileTaskManagerImpl::Create(task_graph_runner_.get());
  }
  void TearDown() override {
    tile_task_manager_->Shutdown();
    tile_task_manager_->CheckForCompletedTasks();

    raster_buffer_provider_->Shutdown();
    resource_pool_.reset();
  }

  // Overridden from PerfRasterBufferProviderHelper:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes) override {
    return raster_buffer_provider_->AcquireBufferForRaster(
        resource, resource_content_id, previous_content_id,
        depends_on_at_raster_decodes,
        false /* depends_on_hardware_accelerated_jpeg_candidates */,
        false /* depends_on_hardware_accelerated_webp_candidates */);
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    task_graph_runner_->RunUntilIdle();
    task_runner_->RunUntilIdle();
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned num_raster_tasks,
                            unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(this, num_raster_tasks, image_decode_tasks,
                      &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      ResetRasterTasks(raster_tasks);
      BuildTileTaskGraph(&graph, raster_tasks);
      tile_task_manager_->ScheduleTasks(&graph);
      tile_task_manager_->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    tile_task_manager_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();
    tile_task_manager_->CheckForCompletedTasks();

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_tasks" + TestModifierString(), timer_.LapsPerSecond());
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     unsigned num_raster_tasks,
                                     unsigned num_image_decode_tasks) {
    const size_t kNumVersions = 2;
    TileTask::Vector image_decode_tasks[kNumVersions];
    RasterTaskVector raster_tasks[kNumVersions];
    for (size_t i = 0; i < kNumVersions; ++i) {
      CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks[i]);
      CreateRasterTasks(this, num_raster_tasks, image_decode_tasks[i],
                        &raster_tasks[i]);
    }

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    size_t count = 0;
    timer_.Reset();
    do {
      graph.Reset();
      // Reset the tasks as for scheduling new state tasks are needed.
      ResetRasterTasks(raster_tasks[count % kNumVersions]);
      BuildTileTaskGraph(&graph, raster_tasks[count % kNumVersions]);
      tile_task_manager_->ScheduleTasks(&graph);
      tile_task_manager_->CheckForCompletedTasks();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    tile_task_manager_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();
    tile_task_manager_->CheckForCompletedTasks();

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_alternate_tasks" + TestModifierString(),
                       timer_.LapsPerSecond());
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      unsigned num_raster_tasks,
                                      unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(this, num_raster_tasks, image_decode_tasks,
                      &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      tile_task_manager_->ScheduleTasks(&graph);
      RunMessageLoopUntilAllTasksHaveCompleted();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    tile_task_manager_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_and_execute_tasks" + TestModifierString(),
                       timer_.LapsPerSecond());
  }

 protected:
  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("schedule", story_name);
    reporter.RegisterImportantMetric("_tasks" + TestModifierString(), "runs/s");
    reporter.RegisterImportantMetric("_alternate_tasks" + TestModifierString(),
                                     "runs/s");
    reporter.RegisterImportantMetric(
        "_and_execute_tasks" + TestModifierString(), "runs/s");
    return reporter;
  }

 private:
  void Create3dResourceProvider() {
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
  }

  void CreateSoftwareResourceProvider() {
    layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
  }

  std::string TestModifierString() const {
    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        return std::string("_zero_copy_raster_buffer_provider");
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        return std::string("_one_copy_raster_buffer_provider");
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        return std::string("_gpu_raster_buffer_provider");
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        return std::string("_bitmap_raster_buffer_provider");
    }
    NOTREACHED();
  }

  std::unique_ptr<TileTaskManager> tile_task_manager_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  std::unique_ptr<RasterQueryQueue> pending_raster_queries_;
};

TEST_P(RasterBufferProviderPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_0", 1, 0);
  RunScheduleTasksTest("32_0", 32, 0);
  RunScheduleTasksTest("1_1", 1, 1);
  RunScheduleTasksTest("32_1", 32, 1);
  RunScheduleTasksTest("1_4", 1, 4);
  RunScheduleTasksTest("32_4", 32, 4);
}

TEST_P(RasterBufferProviderPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("1_0", 1, 0);
  RunScheduleAlternateTasksTest("32_0", 32, 0);
  RunScheduleAlternateTasksTest("1_1", 1, 1);
  RunScheduleAlternateTasksTest("32_1", 32, 1);
  RunScheduleAlternateTasksTest("1_4", 1, 4);
  RunScheduleAlternateTasksTest("32_4", 32, 4);
}

TEST_P(RasterBufferProviderPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("1_0", 1, 0);
  RunScheduleAndExecuteTasksTest("32_0", 32, 0);
  RunScheduleAndExecuteTasksTest("1_1", 1, 1);
  RunScheduleAndExecuteTasksTest("32_1", 32, 1);
  RunScheduleAndExecuteTasksTest("1_4", 1, 4);
  RunScheduleAndExecuteTasksTest("32_4", 32, 4);
}

INSTANTIATE_TEST_SUITE_P(
    RasterBufferProviderPerfTests,
    RasterBufferProviderPerfTest,
    ::testing::Values(RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
                      RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
                      RASTER_BUFFER_PROVIDER_TYPE_GPU,
                      RASTER_BUFFER_PROVIDER_TYPE_BITMAP));

class RasterBufferProviderCommonPerfTest
    : public RasterBufferProviderPerfTestBase,
      public testing::Test {
 public:
  // Overridden from testing::Test:
  void SetUp() override {
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
    resource_pool_ = std::make_unique<ResourcePool>(
        resource_provider_.get(), compositor_context_provider_.get(),
        task_runner_, ResourcePool::kDefaultExpirationDelay, false);
  }

  void RunBuildTileTaskGraphTest(const std::string& test_name,
                                 unsigned num_raster_tasks,
                                 unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(nullptr, num_raster_tasks, image_decode_tasks,
                      &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    CancelRasterTasks(raster_tasks);

    for (auto& task : raster_tasks)
      task->OnTaskCompleted();

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("", timer_.LapsPerSecond());
  }

 protected:
  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("build_raster_test_graph",
                                           story_name);
    reporter.RegisterImportantMetric("", "runs/s");
    return reporter;
  }
};

TEST_F(RasterBufferProviderCommonPerfTest, BuildTileTaskGraph) {
  RunBuildTileTaskGraphTest("1_0", 1, 0);
  RunBuildTileTaskGraphTest("32_0", 32, 0);
  RunBuildTileTaskGraphTest("1_1", 1, 1);
  RunBuildTileTaskGraphTest("32_1", 32, 1);
  RunBuildTileTaskGraphTest("1_4", 1, 4);
  RunBuildTileTaskGraphTest("32_4", 32, 4);
}

}  // namespace
}  // namespace cc
