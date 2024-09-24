// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_flags.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_scoped_ui_resource.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/fake_scrollbar_layer.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/media.h"

using media::VideoFrame;

namespace cc {
namespace {

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

// These tests deal with losing the 3d graphics context.
class LayerTreeHostContextTest : public LayerTreeTest {
 public:
  LayerTreeHostContextTest()
      : LayerTreeTest(),
        times_to_fail_create_(0),
        times_to_lose_during_commit_(0),
        times_to_lose_during_draw_(0),
        times_to_fail_recreate_(0),
        times_to_expect_create_failed_(0),
        times_create_failed_(0),
        committed_at_least_once_(false),
        fallback_context_works_(false),
        async_layer_tree_frame_sink_creation_(false) {
    media::InitializeMediaLibrary();
  }

  void LoseContext() {
    // CreateDisplayLayerTreeFrameSink happens on a different thread, so lock
    // gl_ to make sure we don't set it to null after recreating it
    // there.
    base::AutoLock lock(gl_lock_);
    // For sanity-checking tests, they should only call this when the
    // context is not lost.
    CHECK(gl_);
    gl_->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                             GL_INNOCENT_CONTEXT_RESET_ARB);
    gl_ = nullptr;
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    base::AutoLock lock(gl_lock_);

    auto gl_owned = std::make_unique<viz::TestGLES2Interface>();

    gl_ = gl_owned.get();

    auto provider = viz::TestContextProvider::Create(std::move(gl_owned));
    if (times_to_fail_create_) {
      --times_to_fail_create_;
      ExpectCreateToFail();
      gl_->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                               GL_INNOCENT_CONTEXT_RESET_ARB);
    }

    sii_ = provider->SharedImageInterface();

    return LayerTreeTest::CreateLayerTreeFrameSink(
        renderer_settings, refresh_rate, std::move(provider),
        std::move(worker_context_provider));
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    if (draw_result == DrawResult::kAbortedMissingHighResContent) {
      // Only valid for single-threaded compositing, which activates
      // immediately and will try to draw again when content has finished.
      DCHECK(!host_impl->task_runner_provider()->HasImplThread());
      return draw_result;
    }
    EXPECT_EQ(DrawResult::kSuccess, draw_result);
    if (!times_to_lose_during_draw_)
      return draw_result;

    --times_to_lose_during_draw_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;

    return draw_result;
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    committed_at_least_once_ = true;

    if (!times_to_lose_during_commit_)
      return;
    --times_to_lose_during_commit_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
  }

  void DidFailToInitializeLayerTreeFrameSink() override {
    ++times_create_failed_;
  }

  void TearDown() override {
    LayerTreeTest::TearDown();
    EXPECT_EQ(times_to_expect_create_failed_, times_create_failed_);
  }

  void ExpectCreateToFail() { ++times_to_expect_create_failed_; }

 protected:
  // Protects use of gl_ so LoseContext and
  // CreateDisplayLayerTreeFrameSink can both use it on different threads.
  base::Lock gl_lock_;
  raw_ptr<viz::TestGLES2Interface, AcrossTasksDanglingUntriaged> gl_ = nullptr;
  raw_ptr<gpu::TestSharedImageInterface, AcrossTasksDanglingUntriaged> sii_ =
      nullptr;

  int times_to_fail_create_;
  int times_to_lose_during_commit_;
  int times_to_lose_during_draw_;
  int times_to_fail_recreate_;
  int times_to_expect_create_failed_;
  int times_create_failed_;
  bool committed_at_least_once_;
  bool fallback_context_works_;
  bool async_layer_tree_frame_sink_creation_;
};

class LayerTreeHostContextTestLostContextSucceeds
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextSucceeds()
      : LayerTreeHostContextTest(),
        test_case_(0),
        num_losses_(0),
        num_losses_last_test_case_(-1),
        recovered_context_(true),
        first_initialized_(false) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void RequestNewLayerTreeFrameSink() override {
    if (async_layer_tree_frame_sink_creation_) {
      MainThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&LayerTreeHostContextTestLostContextSucceeds::
                             AsyncRequestNewLayerTreeFrameSink,
                         base::Unretained(this)));
    } else {
      AsyncRequestNewLayerTreeFrameSink();
    }
  }

  void AsyncRequestNewLayerTreeFrameSink() {
    LayerTreeHostContextTest::RequestNewLayerTreeFrameSink();
  }

  void DidInitializeLayerTreeFrameSink() override {
    if (first_initialized_)
      ++num_losses_;
    else
      first_initialized_ = true;

    recovered_context_ = true;
  }

  void AfterTest() override { EXPECT_EQ(11u, test_case_); }

  void DidCommitAndDrawFrame() override {
    // If the last frame had a context loss, then we'll commit again to
    // recover.
    if (!recovered_context_)
      return;
    if (times_to_lose_during_commit_)
      return;
    if (times_to_lose_during_draw_)
      return;

    recovered_context_ = false;
    if (NextTestCase())
      InvalidateAndSetNeedsCommit();
    else
      EndTest();
  }

  virtual void InvalidateAndSetNeedsCommit() {
    // Cause damage so we try to draw.
    layer_tree_host()->root_layer()->SetNeedsDisplay();
    layer_tree_host()->SetNeedsCommit();
  }

  bool NextTestCase() {
    static const TestCase kTests[] = {
        // Losing the context and failing to recreate it (or losing it again
        // immediately) a small number of times should succeed.
        {
            1,      // times_to_lose_during_commit
            0,      // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            false,  // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            0,      // times_to_lose_during_commit
            1,      // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            false,  // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            1,      // times_to_lose_during_commit
            0,      // times_to_lose_during_draw
            3,      // times_to_fail_recreate
            false,  // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            0,      // times_to_lose_during_commit
            1,      // times_to_lose_during_draw
            3,      // times_to_fail_recreate
            false,  // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            0,      // times_to_lose_during_commit
            1,      // times_to_lose_during_draw
            3,      // times_to_fail_recreate
            false,  // fallback_context_works
            true,   // async_layer_tree_frame_sink_creation
        },
        // Losing the context and recreating it any number of times should
        // succeed.
        {
            10,     // times_to_lose_during_commit
            0,      // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            false,  // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            0,      // times_to_lose_during_commit
            10,     // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            false,  // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            10,     // times_to_lose_during_commit
            0,      // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            false,  // fallback_context_works
            true,   // async_layer_tree_frame_sink_creation
        },
        {
            0,      // times_to_lose_during_commit
            10,     // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            false,  // fallback_context_works
            true,   // async_layer_tree_frame_sink_creation
        },
        // Losing the context, failing to reinitialize it, and making a fallback
        // context should work.
        {
            0,      // times_to_lose_during_commit
            1,      // times_to_lose_during_draw
            0,      // times_to_fail_recreate
            true,   // fallback_context_works
            false,  // async_layer_tree_frame_sink_creation
        },
        {
            0,     // times_to_lose_during_commit
            1,     // times_to_lose_during_draw
            0,     // times_to_fail_recreate
            true,  // fallback_context_works
            true,  // async_layer_tree_frame_sink_creation
        },
    };

    if (test_case_ >= std::size(kTests))
      return false;
    // Make sure that we lost our context at least once in the last test run so
    // the test did something.
    EXPECT_GT(num_losses_, num_losses_last_test_case_);
    num_losses_last_test_case_ = num_losses_;

    times_to_lose_during_commit_ =
        kTests[test_case_].times_to_lose_during_commit;
    times_to_lose_during_draw_ = kTests[test_case_].times_to_lose_during_draw;
    times_to_fail_recreate_ = kTests[test_case_].times_to_fail_recreate;
    fallback_context_works_ = kTests[test_case_].fallback_context_works;
    async_layer_tree_frame_sink_creation_ =
        kTests[test_case_].async_layer_tree_frame_sink_creation;
    ++test_case_;
    return true;
  }

  struct TestCase {
    int times_to_lose_during_commit;
    int times_to_lose_during_draw;
    int times_to_fail_recreate;
    bool fallback_context_works;
    bool async_layer_tree_frame_sink_creation;
  };

 protected:
  size_t test_case_;
  int num_losses_;
  int num_losses_last_test_case_;
  bool recovered_context_;
  bool first_initialized_;
};

// Disabled because of crbug.com/736392
// SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLostContextSucceeds);

class LayerTreeHostClientNotVisibleDoesNotCreateLayerTreeFrameSink
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostClientNotVisibleDoesNotCreateLayerTreeFrameSink()
      : LayerTreeHostContextTest() {}

  void WillBeginTest() override {
    // Override to not become visible.
    DCHECK(!layer_tree_host()->IsVisible());
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();
    EndTest();
  }

  void RequestNewLayerTreeFrameSink() override {
    ADD_FAILURE() << "RequestNewLayerTreeFrameSink() should not be called";
  }

  void DidInitializeLayerTreeFrameSink() override { EXPECT_TRUE(false); }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostClientNotVisibleDoesNotCreateLayerTreeFrameSink);

// This tests the LayerTreeFrameSink release logic in the following sequence.
// SetUp LTH and create and init LayerTreeFrameSink.
// LTH::SetVisible(false);
// LTH::ReleaseLayerTreeFrameSink();
// ...
// LTH::SetVisible(true);
// Create and init new LayerTreeFrameSink
class LayerTreeHostClientTakeAwayLayerTreeFrameSink
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostClientTakeAwayLayerTreeFrameSink()
      : LayerTreeHostContextTest(), setos_counter_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void RequestNewLayerTreeFrameSink() override {
    if (layer_tree_host()->IsVisible()) {
      setos_counter_++;
      LayerTreeHostContextTest::RequestNewLayerTreeFrameSink();
    }
  }

  void HideAndReleaseLayerTreeFrameSink() {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    layer_tree_host()->SetVisible(false);
    std::unique_ptr<LayerTreeFrameSink> surface =
        layer_tree_host()->ReleaseLayerTreeFrameSink();
    CHECK(surface);
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LayerTreeHostClientTakeAwayLayerTreeFrameSink::MakeVisible,
            base::Unretained(this)));
  }

  void DidInitializeLayerTreeFrameSink() override {
    EXPECT_TRUE(layer_tree_host()->IsVisible());
    if (setos_counter_ == 1) {
      MainThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&LayerTreeHostClientTakeAwayLayerTreeFrameSink::
                             HideAndReleaseLayerTreeFrameSink,
                         base::Unretained(this)));
    } else {
      EndTest();
    }
  }

  void MakeVisible() {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    layer_tree_host()->SetVisible(true);
  }

  int setos_counter_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostClientTakeAwayLayerTreeFrameSink);

class MultipleCompositeDoesNotCreateLayerTreeFrameSink
    : public LayerTreeHostContextTest {
 public:
  MultipleCompositeDoesNotCreateLayerTreeFrameSink()
      : LayerTreeHostContextTest(), request_count_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->single_thread_proxy_scheduler = false;
    settings->use_zero_copy = true;
  }

  void RequestNewLayerTreeFrameSink() override {
    EXPECT_GE(1, ++request_count_);
    EndTest();
  }

  void BeginTest() override {
    layer_tree_host()->CompositeForTest(TicksFromMicroseconds(1), false,
                                        base::OnceClosure());
    layer_tree_host()->CompositeForTest(TicksFromMicroseconds(2), false,
                                        base::OnceClosure());
  }

  void DidInitializeLayerTreeFrameSink() override { EXPECT_TRUE(false); }

  int request_count_;
};

// This test uses Composite() which only exists for single thread.
SINGLE_THREAD_TEST_F(MultipleCompositeDoesNotCreateLayerTreeFrameSink);

// This test makes sure that once a SingleThreadProxy issues a
// DidFailToInitializeLayerTreeFrameSink, that future Composite calls will not
// trigger additional requests for output surfaces.
class FailedCreateDoesNotCreateExtraLayerTreeFrameSink
    : public LayerTreeHostContextTest {
 public:
  FailedCreateDoesNotCreateExtraLayerTreeFrameSink()
      : LayerTreeHostContextTest(), num_requests_(0), has_failed_(false) {
    times_to_fail_create_ = 1;
  }

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->single_thread_proxy_scheduler = false;
    settings->use_zero_copy = true;
  }

  void RequestNewLayerTreeFrameSink() override {
    num_requests_++;
    // There should be one initial request and then one request from
    // the LayerTreeTest test hooks DidFailToInitializeLayerTreeFrameSink
    // (which is hard to skip).  This second request is just ignored and is test
    // cruft.
    EXPECT_LE(num_requests_, 2);
    if (num_requests_ > 1)
      return;
    LayerTreeHostContextTest::RequestNewLayerTreeFrameSink();
  }

  void BeginTest() override {
    // First composite tries to create a surface.
    layer_tree_host()->CompositeForTest(TicksFromMicroseconds(1), false,
                                        base::OnceClosure());
    EXPECT_EQ(num_requests_, 2);
    EXPECT_TRUE(has_failed_);

    // Second composite should not request or fail.
    layer_tree_host()->CompositeForTest(TicksFromMicroseconds(2), false,
                                        base::OnceClosure());
    EXPECT_EQ(num_requests_, 2);
    EndTest();
  }

  void DidInitializeLayerTreeFrameSink() override { EXPECT_TRUE(false); }

  void DidFailToInitializeLayerTreeFrameSink() override {
    LayerTreeHostContextTest::DidFailToInitializeLayerTreeFrameSink();
    EXPECT_FALSE(has_failed_);
    has_failed_ = true;
  }

  int num_requests_;
  bool has_failed_;
};

// This test uses Composite() which only exists for single thread.
SINGLE_THREAD_TEST_F(FailedCreateDoesNotCreateExtraLayerTreeFrameSink);

// This test uses PictureLayer to check for a working context.
class LayerTreeHostContextTestLostContextSucceedsWithContent
    : public LayerTreeHostContextTestLostContextSucceeds {
 public:
  void SetupTree() override {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetIsDrawable(true);

    // Paint non-solid color.
    PaintFlags flags;
    flags.setColor(SkColorSetARGB(100, 80, 200, 200));
    client_.add_draw_rect(gfx::Rect(5, 5), flags);

    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(10, 10));
    layer_->SetIsDrawable(true);

    root_->AddChild(layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void InvalidateAndSetNeedsCommit() override {
    // Invalidate the render surface so we don't try to use a cached copy of the
    // surface.  We want to make sure to test the drawing paths for drawing to
    // a child surface.
    layer_->SetNeedsDisplay();
    LayerTreeHostContextTestLostContextSucceeds::InvalidateAndSetNeedsCommit();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    FakePictureLayerImpl* picture_impl = static_cast<FakePictureLayerImpl*>(
        host_impl->active_tree()->LayerById(layer_->id()));
    EXPECT_TRUE(picture_impl->HighResTiling()
                    ->TileAt(0, 0)
                    ->draw_info()
                    .IsReadyToDraw());
  }

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLostContextSucceedsWithContent);

class LayerTreeHostContextTestCreateLayerTreeFrameSinkFailsOnce
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestCreateLayerTreeFrameSinkFailsOnce()
      : times_to_fail_(1), times_initialized_(0) {
    times_to_fail_create_ = times_to_fail_;
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidInitializeLayerTreeFrameSink() override { times_initialized_++; }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override { EndTest(); }

  void AfterTest() override {
    EXPECT_EQ(times_to_fail_, times_create_failed_);
    EXPECT_NE(0, times_initialized_);
  }

 private:
  int times_to_fail_;
  int times_initialized_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestCreateLayerTreeFrameSinkFailsOnce);

class LayerTreeHostContextTestLostContextAndEvictTextures
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextAndEvictTextures()
      : LayerTreeHostContextTest(),
        impl_host_(nullptr),
        num_commits_(0),
        lost_context_(false) {}

  void SetupTree() override {
    // Paint non-solid color.
    PaintFlags flags;
    flags.setColor(SkColorSetARGB(100, 80, 200, 200));
    client_.add_draw_rect(gfx::Rect(5, 5), flags);

    scoped_refptr<FakePictureLayer> picture_layer =
        FakePictureLayer::Create(&client_);
    picture_layer->SetBounds(gfx::Size(10, 20));
    client_.set_bounds(picture_layer->bounds());
    layer_tree_host()->SetRootLayer(picture_layer);

    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void PostEvictTextures() {
    if (HasImplThread()) {
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&LayerTreeHostContextTestLostContextAndEvictTextures::
                             EvictTexturesOnImplThread,
                         base::Unretained(this)));
    } else {
      DebugScopedSetImplThread impl(task_runner_provider());
      EvictTexturesOnImplThread();
    }
  }

  void EvictTexturesOnImplThread() {
    impl_host_->EvictTexturesForTesting();

    if (lose_after_evict_) {
      LoseContext();
      lost_context_ = true;
    }
  }

  void DidCommitAndDrawFrame() override {
    if (num_commits_ > 1)
      return;
    PostEvictTextures();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    if (num_commits_ > 1)
      return;
    ++num_commits_;
    if (!lose_after_evict_) {
      LoseContext();
      lost_context_ = true;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(impl->active_tree()->root_layer());
    EXPECT_TRUE(picture_impl->HighResTiling()
                    ->TileAt(0, 0)
                    ->draw_info()
                    .IsReadyToDraw());

    impl_host_ = impl;
    if (lost_context_)
      EndTest();
  }

  void DidInitializeLayerTreeFrameSink() override {}

 protected:
  bool lose_after_evict_;
  FakeContentLayerClient client_;
  raw_ptr<LayerTreeHostImpl, AcrossTasksDanglingUntriaged> impl_host_;
  int num_commits_;
  bool lost_context_;
};

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_SingleThread) {
  lose_after_evict_ = true;
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread) {
  lose_after_evict_ = true;
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread) {
  lose_after_evict_ = false;
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread) {
  lose_after_evict_ = false;
  RunTest(CompositorMode::THREADED);
}

class LayerTreeHostContextTestLayersNotified : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLayersNotified()
      : LayerTreeHostContextTest(), num_commits_(0) {}

  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    child_ = FakePictureLayer::Create(&client_);
    grandchild_ = FakePictureLayer::Create(&client_);

    root_->AddChild(child_);
    child_->AddChild(grandchild_);

    LayerTreeHostContextTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void AttachTree() { layer_tree_host()->SetRootLayer(root_); }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    LayerTreeHostContextTest::DidActivateTreeOnThread(host_impl);

    ++num_commits_;

    FakePictureLayerImpl* root_picture = nullptr;
    FakePictureLayerImpl* child_picture = nullptr;
    FakePictureLayerImpl* grandchild_picture = nullptr;
    // Root layer isn't attached on first activation so the static_cast will
    // fail before second activation.
    if (num_commits_ >= 2) {
      root_picture = static_cast<FakePictureLayerImpl*>(
          host_impl->active_tree()->root_layer());
      child_picture = static_cast<FakePictureLayerImpl*>(
          host_impl->active_tree()->LayerById(child_->id()));
      grandchild_picture = static_cast<FakePictureLayerImpl*>(
          host_impl->active_tree()->LayerById(grandchild_->id()));
    }
    switch (num_commits_) {
      case 1:
        // Because setting the colorspace on the first activation releases
        // resources, don't attach the layers until the first activation.
        // Because of single thread vs multi thread differences (i.e.
        // commit to active tree), if this delay is not done, then the
        // active tree layers will have a different number of resource
        // releasing.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostContextTestLayersNotified::AttachTree,
                           base::Unretained(this)));
        break;
      case 2:
        EXPECT_EQ(0u, root_picture->release_resources_count());
        EXPECT_EQ(0u, child_picture->release_resources_count());
        EXPECT_EQ(0u, grandchild_picture->release_resources_count());

        // Lose the context and struggle to recreate it.
        LoseContext();
        times_to_fail_create_ = 1;
        break;
      case 3:
        EXPECT_TRUE(root_picture->release_resources_count());
        EXPECT_TRUE(child_picture->release_resources_count());
        EXPECT_TRUE(grandchild_picture->release_resources_count());

        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  int num_commits_;

  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> child_;
  scoped_refptr<Layer> grandchild_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLayersNotified);

class LayerTreeHostContextTestDontUseLostResources
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestDontUseLostResources() : lost_context_(false) {
    child_context_provider_ = viz::TestContextProvider::CreateRaster();
    auto result = child_context_provider_->BindToCurrentSequence();
    CHECK_EQ(result, gpu::ContextResult::kSuccess);
    child_resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
  }

  static void EmptyReleaseCallback(const gpu::SyncToken& sync_token,
                                   bool lost) {}

  void SetupTree() override {
    auto* ri = child_context_provider_->RasterInterface();

    scoped_refptr<gpu::ClientSharedImage> shared_image =
        gpu::ClientSharedImage::CreateForTesting();

    gpu::SyncToken sync_token;
    ri->GenSyncTokenCHROMIUM(sync_token.GetData());

    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client_);
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetIsDrawable(true);
    root->AddChild(layer);

    scoped_refptr<TextureLayer> texture =
        TextureLayer::CreateForMailbox(nullptr);
    texture->SetBounds(gfx::Size(10, 10));
    texture->SetIsDrawable(true);
    constexpr gfx::Size size(64, 64);
    auto resource = viz::TransferableResource::MakeGpu(
        shared_image, GL_TEXTURE_2D, sync_token, size,
        viz::SinglePlaneFormat::kRGBA_8888, false /* is_overlay_candidate */);
    texture->SetTransferableResource(
        resource, base::BindOnce(&LayerTreeHostContextTestDontUseLostResources::
                                     EmptyReleaseCallback));
    root->AddChild(texture);

    scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client_);
    mask->SetBounds(gfx::Size(10, 10));
    client_.set_bounds(mask->bounds());

    scoped_refptr<PictureLayer> layer_with_mask =
        PictureLayer::Create(&client_);
    layer_with_mask->SetBounds(gfx::Size(10, 10));
    layer_with_mask->SetIsDrawable(true);
    layer_with_mask->SetMaskLayer(mask);
    root->AddChild(layer_with_mask);

    scoped_refptr<VideoLayer> video_color =
        VideoLayer::Create(&color_frame_provider_, media::VIDEO_ROTATION_0);
    video_color->SetBounds(gfx::Size(10, 10));
    video_color->SetIsDrawable(true);
    root->AddChild(video_color);

    scoped_refptr<VideoLayer> video_hw =
        VideoLayer::Create(&hw_frame_provider_, media::VIDEO_ROTATION_0);
    video_hw->SetBounds(gfx::Size(10, 10));
    video_hw->SetIsDrawable(true);
    root->AddChild(video_hw);

    scoped_refptr<VideoLayer> video_scaled_hw =
        VideoLayer::Create(&scaled_hw_frame_provider_, media::VIDEO_ROTATION_0);
    video_scaled_hw->SetBounds(gfx::Size(10, 10));
    video_scaled_hw->SetIsDrawable(true);
    root->AddChild(video_scaled_hw);

    color_video_frame_ = VideoFrame::CreateColorFrame(
        gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
    ASSERT_TRUE(color_video_frame_);
    hw_video_frame_ = VideoFrame::WrapSharedImage(
        media::PIXEL_FORMAT_ARGB, shared_image, sync_token,
        media::VideoFrame::ReleaseMailboxCB(), gfx::Size(4, 4),
        gfx::Rect(0, 0, 4, 4), gfx::Size(4, 4), base::TimeDelta());
    ASSERT_TRUE(hw_video_frame_);
    scaled_hw_video_frame_ = VideoFrame::WrapSharedImage(
        media::PIXEL_FORMAT_ARGB, shared_image, sync_token,
        media::VideoFrame::ReleaseMailboxCB(), gfx::Size(4, 4),
        gfx::Rect(0, 0, 3, 2), gfx::Size(4, 4), base::TimeDelta());
    ASSERT_TRUE(scaled_hw_video_frame_);

    color_frame_provider_.set_frame(color_video_frame_);
    hw_frame_provider_.set_frame(hw_video_frame_);
    scaled_hw_frame_provider_.set_frame(scaled_hw_video_frame_);

    // Enable the hud.
    LayerTreeDebugState debug_state;
    debug_state.show_property_changed_rects = true;
    layer_tree_host()->SetDebugState(debug_state);

    scoped_refptr<PaintedScrollbarLayer> scrollbar =
        PaintedScrollbarLayer::Create(base::MakeRefCounted<FakeScrollbar>());
    scrollbar->SetScrollElementId(layer->element_id());
    scrollbar->SetBounds(gfx::Size(10, 10));
    scrollbar->SetIsDrawable(true);
    root->AddChild(scrollbar);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    if (host_impl->active_tree()->source_frame_number() == 3) {
      // On the third commit we're recovering from context loss. Hardware
      // video frames should not be reused by the VideoFrameProvider, but
      // software frames can be.
      hw_frame_provider_.set_frame(nullptr);
      scaled_hw_frame_provider_.set_frame(nullptr);
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() == 2) {
      // Lose the context after draw on the second commit. This will cause
      // a third commit to recover.
      LoseContext();
    }
  }

  void RequestNewLayerTreeFrameSink() override {
    // This will get called twice:
    // First when we create the initial LayerTreeFrameSink...
    if (layer_tree_host()->SourceFrameNumber() > 0) {
      // ... and then again after we forced the context to be lost.
      lost_context_ = true;
    }
    LayerTreeHostContextTest::RequestNewLayerTreeFrameSink();
  }

  void DidCommitAndDrawFrame() override {
    ASSERT_TRUE(layer_tree_host()->hud_layer());
    // End the test once we know the 3nd frame drew.
    if (layer_tree_host()->SourceFrameNumber() < 5) {
      layer_tree_host()->root_layer()->SetNeedsDisplay();
      layer_tree_host()->SetNeedsCommit();
    } else {
      EndTest();
    }
  }

  void AfterTest() override { EXPECT_TRUE(lost_context_); }

 private:
  FakeContentLayerClient client_;
  bool lost_context_;

  scoped_refptr<viz::TestContextProvider> child_context_provider_;
  std::unique_ptr<viz::ClientResourceProvider> child_resource_provider_;

  scoped_refptr<VideoFrame> color_video_frame_;
  scoped_refptr<VideoFrame> hw_video_frame_;
  scoped_refptr<VideoFrame> scaled_hw_video_frame_;

  FakeVideoFrameProvider color_frame_provider_;
  FakeVideoFrameProvider hw_frame_provider_;
  FakeVideoFrameProvider scaled_hw_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestDontUseLostResources);

class LayerTreeHostContextTestImplSidePainting
    : public LayerTreeHostContextTest {
 public:
  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    scoped_refptr<PictureLayer> picture = PictureLayer::Create(&client_);
    picture->SetBounds(gfx::Size(10, 10));
    client_.set_bounds(picture->bounds());
    picture->SetIsDrawable(true);
    root->AddChild(picture);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override {
    times_to_lose_during_commit_ = 1;
    PostSetNeedsCommitToMainThread();
  }

  void DidInitializeLayerTreeFrameSink() override { EndTest(); }

 private:
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestImplSidePainting);

class ScrollbarLayerLostContext : public LayerTreeHostContextTest {
 public:
  ScrollbarLayerLostContext() : commits_(0) {}

  void SetupTree() override {
    SetInitialRootBounds(gfx::Size(256, 256));
    LayerTreeTest::SetupTree();
  }

  void BeginTest() override {
    scoped_refptr<Layer> scroll_layer = Layer::Create();
    scrollbar_layer_ = base::MakeRefCounted<FakePaintedScrollbarLayer>(
        scroll_layer->element_id());
    scrollbar_layer_->SetBounds(gfx::Size(10, 100));
    layer_tree_host()->root_layer()->AddChild(scrollbar_layer_);
    layer_tree_host()->root_layer()->AddChild(scroll_layer);
    PostSetNeedsCommitToMainThread();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);

    ++commits_;
    switch (commits_) {
      case 1:
        // First (regular) update, we should upload 2 resources (thumb, and
        // backtrack).
        EXPECT_EQ(1, scrollbar_layer_->update_count());
        LoseContext();
        break;
      case 2:
        // Second update, after the lost context, we should still upload 2
        // resources even if the contents haven't changed.
        EXPECT_EQ(2, scrollbar_layer_->update_count());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  int commits_;
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(ScrollbarLayerLostContext);

class UIResourceLostTest : public LayerTreeHostContextTest {
 public:
  UIResourceLostTest() : time_step_(0) {}
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  // This is called on the main thread after each commit and
  // DidActivateTreeOnThread, with the value of time_step_ at the time
  // of the call to DidActivateTreeOnThread. Similar tests will do
  // work on the main thread in DidCommit but that is unsuitable because
  // the main thread work for these tests must happen after
  // DidActivateTreeOnThread, which happens after DidCommit with impl-side
  // painting.
  virtual void StepCompleteOnMainThread(int time_step) = 0;

  // Called after DidActivateTreeOnThread. If this is done during the commit,
  // the call to StepCompleteOnMainThread will not occur until after
  // the commit completes, because the main thread is blocked.
  void PostStepCompleteToMainThread() {
    task_runner_provider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&UIResourceLostTest::StepCompleteOnMainThreadInternal,
                       base::Unretained(this), time_step_));
  }

  void PostLoseContextToImplThread() {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&LayerTreeHostContextTest::LoseContext,
                                  base::Unretained(this)));
  }

 protected:
  int time_step_;
  std::unique_ptr<FakeScopedUIResource> ui_resource_;

 private:
  void StepCompleteOnMainThreadInternal(int step) {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    StepCompleteOnMainThread(step);
  }
};

class UIResourceLostTestSimple : public UIResourceLostTest {
 public:
  // This is called when the new layer tree has been activated.
  virtual void StepCompleteOnImplThread(LayerTreeHostImpl* impl) = 0;

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    StepCompleteOnImplThread(impl);
    PostStepCompleteToMainThread();
    ++time_step_;
  }
};

// Losing context after an UI resource has been created.
class UIResourceLostAfterCommit : public UIResourceLostTestSimple {
 public:
  void StepCompleteOnMainThread(int step) override {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        // Expects a valid UIResourceId.
        EXPECT_NE(0, ui_resource_->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 5:
        NOTREACHED();
    }
  }

  void StepCompleteOnImplThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // The resource should have been created on LTHI after the commit.
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        LoseContext();
        break;
      case 3:
        // The resources should have been recreated. The bitmap callback should
        // have been called once with the resource_lost flag set to true.
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        // Resource Id on the impl-side have been recreated as well. Note
        // that the same UIResourceId persists after the context lost.
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        PostSetNeedsCommitToMainThread();
        break;
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostAfterCommit);

// Losing context before UI resource requests can be commited.  Three sequences
// of creation/deletion are considered:
// 1. Create one resource -> Context Lost => Expect the resource to have been
// created.
// 2. Delete an existing resource (test_id0_) -> create a second resource
// (test_id1_) -> Context Lost => Expect the test_id0_ to be removed and
// test_id1_ to have been created.
// 3. Create one resource -> Delete that same resource -> Context Lost => Expect
// the resource to not exist in the manager.
class UIResourceLostBeforeCommit : public UIResourceLostTestSimple {
 public:
  UIResourceLostBeforeCommit() : test_id0_(0), test_id1_(0) {}

  void StepCompleteOnMainThread(int step) override {
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        // Lose the context on the impl thread before the commit.
        PostLoseContextToImplThread();
        break;
      case 2:
        // Sequence 2:
        // Currently one resource has been created.
        test_id0_ = ui_resource_->id();
        // Delete this resource.
        ui_resource_ = nullptr;
        // Create another resource.
        ui_resource_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        test_id1_ = ui_resource_->id();
        // Sanity check that two resource creations return different ids.
        EXPECT_NE(test_id0_, test_id1_);
        // Lose the context on the impl thread before the commit.
        PostLoseContextToImplThread();
        break;
      case 3:
        // Clear the manager of resources.
        ui_resource_ = nullptr;
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Sequence 3:
        ui_resource_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        test_id0_ = ui_resource_->id();
        // Sanity check the UIResourceId should not be 0.
        EXPECT_NE(0, test_id0_);
        // Usually ScopedUIResource are deleted from the manager in their
        // destructor (so usually ui_resource_ = nullptr).  But here we need
        // ui_resource_ for the next step, so call DeleteUIResource directly.
        layer_tree_host()->GetUIResourceManager()->DeleteUIResource(test_id0_);
        // Delete the resouce and then lose the context.
        PostLoseContextToImplThread();
        break;
      case 5:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 6:
        NOTREACHED();
    }
  }

  void StepCompleteOnImplThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // Sequence 1 (continued):
        // The first context lost happens before the resources were created,
        // and because it resulted in no resources being destroyed, it does not
        // trigger resource re-creation.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // Resource Id on the impl-side has been created.
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        // Sequence 2 (continued):
        // The previous resource should have been deleted.
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(test_id0_));
        // The second resource should have been created.
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(test_id1_));
        // The second resource was not actually uploaded before the context
        // was lost, so it only got created once.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        break;
      case 5:
        // Sequence 3 (continued):
        // Expect the resource callback to have been called once.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        // No "resource lost" callbacks.
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // The UI resource id should not be valid
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(test_id0_));
        break;
    }
  }

 private:
  UIResourceId test_id0_;
  UIResourceId test_id1_;
};

// http://crbug.com/803532 : SINGLE_THREAD_TEST_F is flaky on every bot
MULTI_THREAD_TEST_F(UIResourceLostBeforeCommit);

// Losing UI resource before the pending trees is activated but after the
// commit.  Impl-side-painting only.
class UIResourceLostBeforeActivateTree : public UIResourceLostTest {
  void StepCompleteOnMainThread(int step) override {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        test_id_ = ui_resource_->id();
        ui_resource_ = nullptr;
        PostSetNeedsCommitToMainThread();
        break;
      case 5:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 6:
        // Make sure no extra commits happened.
        NOTREACHED();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 2:
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        PostSetNeedsCommitToMainThread();
        break;
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    switch (time_step_) {
      case 1:
        // The resource creation callback has been called.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        // The resource is not yet lost (sanity check).
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // The resource should not have been created yet on the impl-side.
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        LoseContext();
        break;
      case 3:
        LoseContext();
        break;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::DidActivateTreeOnThread(impl);
    switch (time_step_) {
      case 1:
        // The pending requests on the impl-side should not have been processed
        // since the context was lost. But we should have marked the resource as
        // evicted instead.
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_TRUE(impl->EvictedUIResourcesExist());
        break;
      case 2:
        // The "lost resource" callback should have been called once and it
        // should have gotten recreated now and shouldn't be marked as evicted
        // anymore.
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_FALSE(impl->EvictedUIResourcesExist());
        break;
      case 4:
        // The resource is deleted and should not be in the manager.  Use
        // test_id_ since ui_resource_ has been deleted.
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(test_id_));
        break;
    }

    PostStepCompleteToMainThread();
    ++time_step_;
  }

 private:
  UIResourceId test_id_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostBeforeActivateTree);

// Resources evicted explicitly and by visibility changes.
class UIResourceLostEviction : public UIResourceLostTestSimple {
 public:
  void StepCompleteOnMainThread(int step) override {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        ui_resource2_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        EXPECT_NE(0, ui_resource_->id());
        EXPECT_NE(0, ui_resource2_->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Make the tree not visible.
        PostSetVisibleToMainThread(false);
        ui_resource2_->DeleteResource();
        ui_resource3_ = FakeScopedUIResource::Create(
            layer_tree_host()->GetUIResourceManager());
        break;
      case 3:
        // Release resources before ending the test.
        ui_resource_ = nullptr;
        ui_resource2_ = nullptr;
        ui_resource3_ = nullptr;
        EndTest();
        break;
      case 4:
        NOTREACHED();
    }
  }

  void DidSetVisibleOnImplTree(LayerTreeHostImpl* impl, bool visible) override {
    if (!visible) {
      // All resources should have been evicted.
      ASSERT_EQ(0u, sii_->shared_image_count());
      EXPECT_EQ(viz::kInvalidResourceId,
                impl->ResourceIdForUIResource(ui_resource_->id()));
      EXPECT_EQ(viz::kInvalidResourceId,
                impl->ResourceIdForUIResource(ui_resource2_->id()));
      EXPECT_EQ(viz::kInvalidResourceId,
                impl->ResourceIdForUIResource(ui_resource3_->id()));
      EXPECT_EQ(2, ui_resource_->resource_create_count);
      EXPECT_EQ(1, ui_resource_->lost_resource_count);
      // Drawing is disabled both because of the evicted resources and
      // because the renderer is not visible.
      EXPECT_FALSE(impl->CanDraw());
      // Make the renderer visible again.
      PostSetVisibleToMainThread(true);
    }
  }

  void StepCompleteOnImplThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // The first two resources should have been created on LTHI after the
        // commit.
        ASSERT_EQ(2u, sii_->shared_image_count());
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource2_->id()));
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        // Evict all UI resources. This will trigger a commit.
        impl->EvictAllUIResources();
        ASSERT_EQ(0u, sii_->shared_image_count());
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource2_->id()));
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        EXPECT_FALSE(impl->CanDraw());
        break;
      case 2:
        // The first two resources should have been recreated.
        ASSERT_EQ(2u, sii_->shared_image_count());
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(2, ui_resource_->resource_create_count);
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource2_->id()));
        EXPECT_EQ(2, ui_resource2_->resource_create_count);
        EXPECT_EQ(1, ui_resource2_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        break;
      case 3:
        // The first resource should have been recreated after visibility was
        // restored.
        ASSERT_EQ(2u, sii_->shared_image_count());
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(3, ui_resource_->resource_create_count);
        EXPECT_EQ(2, ui_resource_->lost_resource_count);

        // This resource was deleted.
        EXPECT_EQ(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource2_->id()));
        EXPECT_EQ(2, ui_resource2_->resource_create_count);
        EXPECT_EQ(1, ui_resource2_->lost_resource_count);

        // This resource should have been created now.
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource3_->id()));
        EXPECT_EQ(1, ui_resource3_->resource_create_count);
        EXPECT_EQ(0, ui_resource3_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        break;
    }
  }

 private:
  std::unique_ptr<FakeScopedUIResource> ui_resource2_;
  std::unique_ptr<FakeScopedUIResource> ui_resource3_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostEviction);

class UIResourceFreedIfLostWhileExported : public LayerTreeHostContextTest {
 protected:
  void BeginTest() override {
    // Make 1 UIResource, post it to the compositor thread, where it will be
    // uploaded.
    ui_resource_ =
        FakeScopedUIResource::Create(layer_tree_host()->GetUIResourceManager());
    EXPECT_NE(0, ui_resource_->id());
    PostSetNeedsCommitToMainThread();
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // The UIResource has been created and a gpu resource made for it.
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(1u, sii_->shared_image_count());
        // Lose the LayerTreeFrameSink connection. The UI resource should
        // be replaced and the old texture should be destroyed.
        impl->DidLoseLayerTreeFrameSink();
        break;
      case 1:
        // The UIResource has been recreated, the old texture is not kept
        // around.
        EXPECT_NE(viz::kInvalidResourceId,
                  impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(1u, sii_->shared_image_count());
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &UIResourceFreedIfLostWhileExported::DeleteAndEndTest,
                base::Unretained(this)));
    }
  }

  void DeleteAndEndTest() {
    ui_resource_->DeleteResource();
    EndTest();
  }

  std::unique_ptr<FakeScopedUIResource> ui_resource_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceFreedIfLostWhileExported);

class TileResourceFreedIfLostWhileExported : public LayerTreeHostContextTest {
 protected:
  void SetupTree() override {
    PaintFlags flags;
    client_.set_fill_with_nonsolid_color(true);

    scoped_refptr<FakePictureLayer> picture_layer =
        FakePictureLayer::Create(&client_);
    picture_layer->SetBounds(gfx::Size(10, 20));
    client_.set_bounds(picture_layer->bounds());
    layer_tree_host()->SetRootLayer(std::move(picture_layer));

    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    auto* context_provider = static_cast<viz::TestContextProvider*>(
        impl->layer_tree_frame_sink()->worker_context_provider());
    gpu::TestSharedImageInterface* sii =
        context_provider->SharedImageInterface();
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // The PicturLayer has a texture for a tile, that has been exported to
        // the display compositor now.
        EXPECT_EQ(1u, impl->resource_provider()->num_resources_for_testing());
        EXPECT_EQ(1u, impl->resource_pool()->resource_count());
        // Shows that the tile texture is allocated with the current worker
        // context.
        num_textures_ = sii->shared_image_count();
        EXPECT_GT(num_textures_, 0u);

        // Lose the LayerTreeFrameSink connection. The tile resource should
        // be replaced and the old texture should be destroyed.
        LoseContext();
        break;
      case 1:
        // The tile has been recreated, the old texture is not kept around in
        // the pool indefinitely. It can be dropped as soon as the context is
        // known to be lost.
        EXPECT_EQ(1u, impl->resource_provider()->num_resources_for_testing());
        EXPECT_EQ(1u, impl->resource_pool()->resource_count());
        // Shows that the replacement tile texture is re-allocated with the
        // current worker context, not just the previous one.
        EXPECT_EQ(num_textures_, sii->shared_image_count());
        EndTest();
    }
  }

  FakeContentLayerClient client_;
  size_t num_textures_ = 0;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TileResourceFreedIfLostWhileExported);

class SoftwareTileResourceFreedIfLostWhileExported : public LayerTreeTest {
 protected:
  SoftwareTileResourceFreedIfLostWhileExported()
      : LayerTreeTest(viz::RendererType::kSoftware) {}

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    // Induce software compositing in cc.
    return LayerTreeTest::CreateLayerTreeFrameSink(
        renderer_settings, refresh_rate, nullptr, nullptr);
  }

  void SetupTree() override {
    PaintFlags flags;
    client_.set_fill_with_nonsolid_color(true);

    scoped_refptr<FakePictureLayer> picture_layer =
        FakePictureLayer::Create(&client_);
    picture_layer->SetBounds(gfx::Size(10, 20));
    client_.set_bounds(picture_layer->bounds());
    layer_tree_host()->SetRootLayer(std::move(picture_layer));

    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        // The PicturLayer has a bitmap for a tile, that has been exported to
        // the display compositor now.
        EXPECT_EQ(1u, impl->resource_provider()->num_resources_for_testing());
        EXPECT_EQ(1u, impl->resource_pool()->resource_count());

        impl->DidLoseLayerTreeFrameSink();
        break;
      }
      case 1: {
        // The tile did not need to be recreated, the same bitmap/resource
        // should be used for it.
        EXPECT_EQ(1u, impl->resource_provider()->num_resources_for_testing());
        EXPECT_EQ(1u, impl->resource_pool()->resource_count());

        // TODO(danakj): It'd be possible to not destroy and recreate the
        // software bitmap, however for simplicity we do the same for software
        // and for gpu paths. If we didn't destroy it we could see the same
        // bitmap on PictureLayerImpl's tile.

        EndTest();
      }
    }
  }

  FakeContentLayerClient client_;
  viz::ResourceId exported_resource_id_ = viz::kInvalidResourceId;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTileResourceFreedIfLostWhileExported);

class LayerTreeHostContextTestLoseAfterSendingBeginMainFrame
    : public LayerTreeHostContextTest {
 protected:
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void WillBeginMainFrame() override {
    // Don't begin a frame with a lost surface.
    EXPECT_FALSE(lost_);

    if (deferred_)
      return;
    deferred_ = true;

    scoped_defer_main_frame_update_ = layer_tree_host()->DeferMainFrameUpdate();
    // Meanwhile, lose the context while we are in defer BeginMainFrame.
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeHostContextTestLoseAfterSendingBeginMainFrame::
                           LoseContextOnImplThread,
                       base::Unretained(this)));

    // After the first frame, we will lose the context and then not start
    // lifecycle updates and commits until that happens. The 2nd frame should
    // not happen before DidInitializeLayerTreeFrameSink occurs.
    lost_ = true;
  }

  void DidInitializeLayerTreeFrameSink() override {
    EXPECT_TRUE(lost_);
    lost_ = false;
  }

  void LoseContextOnImplThread() {
    LoseContext();

    // After losing the context, stop deferring commits.
    PostReturnDeferMainFrameUpdateToMainThread(
        std::move(scoped_defer_main_frame_update_));
  }

  void DidCommitAndDrawFrame() override { EndTest(); }

  std::unique_ptr<ScopedDeferMainFrameUpdate> scoped_defer_main_frame_update_;
  bool deferred_ = false;
  bool lost_ = true;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLoseAfterSendingBeginMainFrame);

class LayerTreeHostContextTestWorkerContextLostRecovery : public LayerTreeTest {
 protected:
  void SetupTree() override {
    PaintFlags flags;
    client_.set_fill_with_nonsolid_color(true);
    client_.add_draw_rect(gfx::Rect(5, 5), flags);

    scoped_refptr<FakePictureLayer> picture_layer =
        FakePictureLayer::Create(&client_);
    picture_layer->SetBounds(gfx::Size(10, 20));
    client_.set_bounds(picture_layer->bounds());
    layer_tree_host()->SetRootLayer(picture_layer);

    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void WillPrepareTilesOnThread(LayerTreeHostImpl* host_impl) override {
    if (did_lose_context)
      return;
    did_lose_context = true;
    viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
        host_impl->layer_tree_frame_sink()->worker_context_provider());
    gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
    ri->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                            GL_INNOCENT_CONTEXT_RESET_ARB);
  }

  void DidInitializeLayerTreeFrameSink() override { num_frame_sinks_++; }

  void DidCommitAndDrawFrame() override { EndTest(); }

  void AfterTest() override {
    EXPECT_TRUE(did_lose_context);
    EXPECT_EQ(num_frame_sinks_, 2);
  }

  FakeContentLayerClient client_;
  bool did_lose_context = false;
  int num_frame_sinks_ = 0;
};

MULTI_THREAD_TEST_F(LayerTreeHostContextTestWorkerContextLostRecovery);

}  // namespace
}  // namespace cc
