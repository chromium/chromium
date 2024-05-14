// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_TEST_H_
#define CC_TEST_LAYER_TREE_TEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation_delegate.h"
#include "cc/base/features.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/test/test_hooks.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_types.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class ScopedFeatureList;
}
}  // namespace base

namespace viz {
class BeginFrameSource;
class TestContextProvider;
}

namespace cc {

class Animation;
class AnimationHost;
class LayerTreeHost;
class LayerTreeHostForTesting;
class LayerTreeTestLayerTreeFrameSinkClient;
class Proxy;
class TestLayerTreeFrameSink;
class TestTaskGraphRunner;

class LayerTreeHostClientForTesting;

// The LayerTreeTests runs with the main loop running. It instantiates a single
// LayerTreeHostForTesting and associated LayerTreeHostImplForTesting and
// LayerTreeHostClientForTesting.
//
// BeginTest() is called once the main message loop is running and the layer
// tree host is initialized.
//
// Key stages of the drawing loop, e.g. drawing or commiting, redirect to
// LayerTreeTest methods of similar names. To track the commit process, override
// these functions.
//
// The test continues until someone calls EndTest. EndTest can be called on any
// thread, but be aware that ending the test is an asynchronous process.
class LayerTreeTest : public testing::Test, public TestHooks {
 public:
  // TODO(kylechar): This shouldn't be SkiaRenderer/GL for platforms with no GL
  // support.
  static constexpr viz::RendererType kDefaultRendererType =
      viz::RendererType::kSkiaGL;

  std::string TestTypeToString() {
    switch (renderer_type_) {
      case viz::RendererType::kSkiaGL:
        return "Skia GL";
      case viz::RendererType::kSkiaVk:
        return "Skia Vulkan";
      case viz::RendererType::kSkiaGraphiteDawn:
        return "Skia Graphite Dawn";
      case viz::RendererType::kSkiaGraphiteMetal:
        return "Skia Graphite Metal";
      case viz::RendererType::kSoftware:
        return "Software";
    }
  }

  ~LayerTreeTest() override;

  virtual void EndTest();
  void EndTestAfterDelayMs(int delay_milliseconds);

  void PostAddNoDamageAnimationToMainThread(
      Animation* animation_to_receive_animation);
  void PostAddOpacityAnimationToMainThread(
      Animation* animation_to_receive_animation);
  void PostAddOpacityAnimationToMainThreadInstantly(
      Animation* animation_to_receive_animation);
  void PostAddOpacityAnimationToMainThreadDelayed(
      Animation* animation_to_receive_animation);
  void PostSetLocalSurfaceIdToMainThread(
      const viz::LocalSurfaceId& local_surface_id);
  void PostRequestNewLocalSurfaceIdToMainThread();
  void PostGetDeferMainFrameUpdateToMainThread(
      std::unique_ptr<ScopedDeferMainFrameUpdate>*
          scoped_defer_main_frame_update);
  void PostReturnDeferMainFrameUpdateToMainThread(
      std::unique_ptr<ScopedDeferMainFrameUpdate>
          scoped_defer_main_frame_update);
  void PostDeferringCommitsStatusToMainThread(bool is_deferring_commits);
  void PostSetNeedsCommitToMainThread();
  void PostSetNeedsUpdateLayersToMainThread();
  void PostSetNeedsRedrawToMainThread();
  void PostSetNeedsRedrawRectToMainThread(const gfx::Rect& damage_rect);
  void PostSetVisibleToMainThread(bool visible);
  void PostSetNeedsCommitWithForcedRedrawToMainThread();
  void PostCompositeImmediatelyToMainThread();
  void PostNextCommitWaitsForActivationToMainThread();

  void DoBeginTest();
  void Timeout();

  AnimationHost* animation_host() const { return animation_host_.get(); }

  void SetUseLayerLists() { settings_.use_layer_lists = true; }

 protected:
  explicit LayerTreeTest(
      viz::RendererType renderer_type = kDefaultRendererType);

  void SkipAllocateInitialLocalSurfaceId();
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const;
  void GenerateNewLocalSurfaceId();

  virtual void InitializeSettings(LayerTreeSettings* settings) {}

  void RealEndTest();

  std::unique_ptr<LayerTreeFrameSink>
  ReleaseLayerTreeFrameSinkOnLayerTreeHost();
  void SetVisibleOnLayerTreeHost(bool visible);
  void SetInitialDeviceScaleFactor(float initial_device_scale_factor) {
    initial_device_scale_factor_ = initial_device_scale_factor;
  }
  // Used when LayerTreeTest::SetupTree() creates the root layer. Not used if
  // the root layer is created before LayerTreeTest::SetupTree() is called.
  // The default is 1x1.
  void SetInitialRootBounds(const gfx::Size& bounds) {
    initial_root_bounds_ = bounds;
  }

  virtual void CleanupBeforeDestroy() {}
  virtual void AfterTest() {}
  virtual void WillBeginTest();
  virtual void BeginTest() = 0;
  virtual void SetupTree();

  virtual void RunTest(CompositorMode mode);

  bool HasImplThread() const { return !!impl_thread_; }
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() {
    return impl_task_runner_.get();
  }
  base::SingleThreadTaskRunner* MainThreadTaskRunner() const {
    return main_task_runner_.get();
  }
  Proxy* proxy();
  TaskRunnerProvider* task_runner_provider() const;
  TaskGraphRunner* task_graph_runner() const {
    return task_graph_runner_.get();
  }
  bool TestEnded() const {
    base::AutoLock hold(test_ended_lock_);
    return ended_;
  }

  LayerTreeHost* layer_tree_host() const;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return gpu_memory_buffer_manager_.get();
  }

  void DestroyLayerTreeHost();

  // By default, output surface recreation is synchronous.
  void RequestNewLayerTreeFrameSink() override;
  // Override this to modify the TestContextProviders before they are bound
  // and used. Override CreateLayerTreeFrameSink() instead if the test does not
  // want to use TestContextProviders.
  virtual void SetUpUnboundContextProviders(
      viz::TestContextProvider* context_provider,
      viz::TestContextProvider* worker_context_provider);
  // Override this and call the base class to change what
  // viz::RasterContextProviders will be used (such as for pixel tests). Or
  // override it and create your own TestLayerTreeFrameSink to control how it is
  // created.
  virtual std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider);
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayControllerOnThread() override;
  std::unique_ptr<viz::SkiaOutputSurface> CreateSkiaOutputSurfaceOnThread(
      viz::DisplayCompositorMemoryAndTaskController*) override;
  std::unique_ptr<viz::OutputSurface> CreateSoftwareOutputSurfaceOnThread()
      override;

  base::SingleThreadTaskRunner* image_worker_task_runner() const {
    return image_worker_->task_runner().get();
  }

  size_t NumCallsToWaitForProtectedSequenceCompletion() const;

  void UseBeginFrameSource(viz::BeginFrameSource* begin_frame_source) {
    begin_frame_source_ = begin_frame_source;
  }

  bool use_software_renderer() const {
    return renderer_type_ == viz::RendererType::kSoftware;
  }
  bool use_skia_vulkan() const {
    return renderer_type_ == viz::RendererType::kSkiaVk;
  }
  bool use_skia_graphite() const {
    return renderer_type_ == viz::RendererType::kSkiaGraphiteDawn ||
           renderer_type_ == viz::RendererType::kSkiaGraphiteMetal;
  }

  const viz::RendererType renderer_type_;

  const viz::DebugRendererSettings debug_settings_;

 private:
  virtual void DispatchAddNoDamageAnimation(
      Animation* animation_to_receive_animation,
      double animation_duration);
  virtual void DispatchAddOpacityAnimation(
      Animation* animation_to_receive_animation,
      double animation_duration);
  void DispatchSetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id);
  void DispatchRequestNewLocalSurfaceId();
  void DispatchGetDeferMainFrameUpdate(
      std::unique_ptr<ScopedDeferMainFrameUpdate>*
          scoped_defer_main_frame_update);
  void DispatchReturnDeferMainFrameUpdate(
      std::unique_ptr<ScopedDeferMainFrameUpdate>
          scoped_defer_main_frame_update);
  void DispatchDeferringCommitsStatus(bool is_deferring_commits);
  void DispatchSetNeedsCommit();
  void DispatchSetNeedsUpdateLayers();
  void DispatchSetNeedsRedraw();
  void DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect);
  void DispatchSetVisible(bool visible);
  void DispatchSetNeedsCommitWithForcedRedraw();
  void DispatchDidAddAnimation();
  void DispatchCompositeImmediately();
  void DispatchNextCommitWaitsForActivation();

  // |scoped_feature_list_| must be the first member to ensure that it is
  // destroyed after any member that might be using it.
  base::test::ScopedFeatureList scoped_feature_list_;
  viz::TestGpuServiceHolder::ScopedResetter gpu_service_resetter_;

  LayerTreeSettings settings_;
  float initial_device_scale_factor_ = 1.f;
  gfx::Size initial_root_bounds_;

  CompositorMode mode_;

  std::unique_ptr<LayerTreeHostClientForTesting> client_;
  std::unique_ptr<LayerTreeHost> layer_tree_host_;
  std::unique_ptr<AnimationHost> animation_host_;

  bool beginning_ = false;
  bool end_when_begin_returns_ = false;
  bool timed_out_ = false;
  bool scheduled_ = false;
  bool started_ = false;

  mutable base::Lock test_ended_lock_;
  bool ended_ = false;

  int timeout_seconds_ = 0;

  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;  // NOT OWNED.

  std::unique_ptr<LayerTreeTestLayerTreeFrameSinkClient>
      layer_tree_frame_sink_client_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner_;
  std::unique_ptr<base::Thread> impl_thread_;
  std::unique_ptr<base::Thread> image_worker_;
  std::unique_ptr<gpu::TestGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<TestTaskGraphRunner> task_graph_runner_;
  base::CancelableOnceClosure timeout_;
  base::OnceClosure quit_closure_;
  scoped_refptr<viz::TestContextProvider> compositor_contexts_;
  bool skip_allocate_initial_local_surface_id_ = false;
  viz::ParentLocalSurfaceIdAllocator allocator_;
  base::WeakPtr<LayerTreeTest> main_thread_weak_ptr_;
  base::WeakPtrFactory<LayerTreeTest> weak_factory_{this};
};

}  // namespace cc

// Do not change this macro to disable a test, it will disable half of
// the unit test suite. Instead, comment out the usage of this macro for
// a specific test name. eg.
// // TODO(crbug.com/abcd): Disabled for some reasons stated here.
// // SINGLE_AND_MULTI_THREAD_TEST_F(SomeRandomTest)
#define SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME)                   \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread_DelegatingRenderer) { \
    RunTest(CompositorMode::SINGLE_THREADED);                     \
  }                                                               \
  class SingleThreadDelegatingImplNeedsSemicolon##TEST_FIXTURE_NAME {}

// Do not change this macro to disable a test, it will disable half of
// the unit test suite. Instead, comment out the usage of this macro for
// a specific test name. eg.
// // TODO(crbug.com/abcd): Disabled for some reasons stated here.
// // SINGLE_AND_MULTI_THREAD_TEST_F(SomeRandomTest)
#define MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)                   \
  TEST_F(TEST_FIXTURE_NAME, RunMultiThread_DelegatingRenderer) { \
    RunTest(CompositorMode::THREADED);                           \
  }                                                              \
  class MultiThreadDelegatingImplNeedsSemicolon##TEST_FIXTURE_NAME {}

// Do not change this macro to disable a test, it will disable half of
// the unit test suite. Instead, comment out the usage of this macro for
// a specific test name. eg.
// // TODO(crbug.com/abcd): Disabled for some reasons stated here.
// // SINGLE_AND_MULTI_THREAD_TEST_F(SomeRandomTest)
#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME);                \
  MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

// Some tests want to control when notify ready for activation occurs,
// but this is not supported in the single-threaded case.
//
// Do not change this macro to disable a test, it will disable half of
// the unit test suite. Instead, comment out the usage of this macro for
// a specific test name. eg.
// // TODO(crbug.com/abcd): Disabled for some reasons stated here.
// // MULTI_THREAD_BLOCKNOTIFY_TEST_F(SomeRandomTest)
#define MULTI_THREAD_BLOCKNOTIFY_TEST_F(TEST_FIXTURE_NAME) \
  MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

#endif  // CC_TEST_LAYER_TREE_TEST_H_
