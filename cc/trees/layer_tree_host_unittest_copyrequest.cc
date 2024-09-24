// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/test/cc_test_suite.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"

namespace cc {

// CompositorMode is declared in the cc namespace, so this function also needs
// to be in the cc namespace.
void PrintTo(CompositorMode mode, std::ostream* os) {
  *os << (mode == CompositorMode::THREADED ? "MultiThreaded"
                                           : "SingleThreaded");
}

namespace {

auto CombineWithCompositorModes(const std::vector<viz::RendererType>& types) {
  return ::testing::Combine(::testing::ValuesIn(types),
                            ::testing::Values(CompositorMode::SINGLE_THREADED,
                                              CompositorMode::THREADED));
}

class LayerTreeHostCopyRequestTest
    : public LayerTreeTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<viz::RendererType, CompositorMode>> {
 public:
  LayerTreeHostCopyRequestTest() : LayerTreeTest(renderer_type()) {}

  viz::RendererType renderer_type() const {
    return ::testing::get<0>(GetParam());
  }

  CompositorMode compositor_mode() const {
    return ::testing::get<1>(GetParam());
  }
};

class LayerTreeHostCopyRequestTestMultipleRequests
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root = FakePictureLayer::Create(&client_);
    root->SetBounds(gfx::Size(20, 20));

    child = FakePictureLayer::Create(&client_);
    child->SetBounds(gfx::Size(10, 10));
    root->AddChild(child);

    grand_child = FakePictureLayer::Create(&client_);
    grand_child->SetBounds(gfx::Size(5, 5));
    child->AddChild(grand_child);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override { WaitForCallback(); }

  void WaitForCallback() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeHostCopyRequestTestMultipleRequests::NextStep,
                       base::Unretained(this)));
  }

  void NextStep() {
    int frame = layer_tree_host()->SourceFrameNumber();
    switch (frame) {
      case 1:
        child->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(&LayerTreeHostCopyRequestTestMultipleRequests::
                               CopyOutputCallback,
                           base::Unretained(this), 0)));
        EXPECT_EQ(0u, callbacks_.size());
        break;
      case 2:
        // This commit is triggered by the copy request having been completed.
        break;
      case 3:
        if (callbacks_.size() < 1u) {
          WaitForCallback();
          return;
        }
        EXPECT_EQ(1u, callbacks_.size());
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[0].ToString());

        child->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(&LayerTreeHostCopyRequestTestMultipleRequests::
                               CopyOutputCallback,
                           base::Unretained(this), 1)));
        root->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(&LayerTreeHostCopyRequestTestMultipleRequests::
                               CopyOutputCallback,
                           base::Unretained(this), 2)));
        grand_child->RequestCopyOfOutput(
            std::make_unique<viz::CopyOutputRequest>(
                viz::CopyOutputRequest::ResultFormat::RGBA,
                viz::CopyOutputRequest::ResultDestination::kSystemMemory,
                base::BindOnce(&LayerTreeHostCopyRequestTestMultipleRequests::
                                   CopyOutputCallback,
                               base::Unretained(this), 3)));
        EXPECT_EQ(1u, callbacks_.size());
        break;
      case 4:
        // This commit is triggered by the copy request having been completed.
        break;
      case 5:
        if (callbacks_.size() < 4u) {
          WaitForCallback();
          return;
        }
        EXPECT_EQ(4u, callbacks_.size());

        // The |child| was copied to a bitmap and passed back in Case 1.
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[0].ToString());

        // The |child| was copied to a bitmap and passed back in Case 2.
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[1].ToString());
        // The |root| was copied to a bitmap and passed back also in Case 2.
        EXPECT_EQ(gfx::Size(20, 20).ToString(), callbacks_[2].ToString());
        // The |grand_child| was copied to a bitmap and passed back in Case 2.
        EXPECT_EQ(gfx::Size(5, 5).ToString(), callbacks_[3].ToString());
        EndTest();
        break;
    }
  }

  void CopyOutputCallback(size_t id,
                          std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    EXPECT_FALSE(result->IsEmpty());
    auto scoped_sk_bitmap = result->ScopedAccessSkBitmap();
    const SkBitmap& bitmap = scoped_sk_bitmap.bitmap();
    EXPECT_TRUE(bitmap.readyToDraw());
    EXPECT_EQ(result->size(), gfx::Size(bitmap.width(), bitmap.height()));
    callbacks_[id] = result->size();
  }

  void AfterTest() override { EXPECT_EQ(4u, callbacks_.size()); }

  std::map<size_t, gfx::Size> callbacks_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root;
  scoped_refptr<FakePictureLayer> child;
  scoped_refptr<FakePictureLayer> grand_child;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestMultipleRequests,
                         CombineWithCompositorModes(viz::GetRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestMultipleRequests);

TEST_P(LayerTreeHostCopyRequestTestMultipleRequests, Test) {
  RunTest(compositor_mode());
}

// These tests run with |out_of_order_callbacks_| set on the TestContextSupport,
// which causes callbacks for sync queries to be sent in reverse order.
class LayerTreeHostCopyRequestTestMultipleRequestsOutOfOrder
    : public LayerTreeHostCopyRequestTestMultipleRequests {
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayControllerOnThread() override {
    // In this implementation, none of the output surface has a real gpu thread,
    // and there is no overlay support.
    return nullptr;
  }

  std::unique_ptr<viz::SkiaOutputSurface> CreateSkiaOutputSurfaceOnThread(
      viz::DisplayCompositorMemoryAndTaskController*) override {
    auto skia_output_surface = viz::FakeSkiaOutputSurface::Create3d();
    skia_output_surface->SetOutOfOrderCallbacks(true);
    return skia_output_surface;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestMultipleRequestsOutOfOrder,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestMultipleRequestsOutOfOrder);

TEST_P(LayerTreeHostCopyRequestTestMultipleRequestsOutOfOrder, Test) {
  RunTest(compositor_mode());
}

// TODO(crbug.com/40447355): Remove this test when the workaround it tests is no
// longer needed.
class LayerTreeHostCopyRequestCompletionCausesCommit
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    int frame = layer_tree_host()->SourceFrameNumber();
    switch (frame) {
      case 1:
        layer_->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(&LayerTreeHostCopyRequestCompletionCausesCommit::
                               CopyOutputCallback)));
        break;
      case 2:
        // This commit is triggered by the copy request.
        break;
      case 3:
        // This commit is triggered by the completion of the copy request.
        EndTest();
        break;
    }
  }

  static void CopyOutputCallback(
      std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestCompletionCausesCommit,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestCompletionCausesCommit);

TEST_P(LayerTreeHostCopyRequestCompletionCausesCommit, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestLayerDestroyed
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    main_destroyed_ = FakePictureLayer::Create(&client_);
    main_destroyed_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(main_destroyed_);

    impl_destroyed_ = FakePictureLayer::Create(&client_);
    impl_destroyed_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(impl_destroyed_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    int frame = layer_tree_host()->SourceFrameNumber();
    switch (frame) {
      case 1:
        main_destroyed_->RequestCopyOfOutput(std::make_unique<
                                             viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(
                &LayerTreeHostCopyRequestTestLayerDestroyed::CopyOutputCallback,
                base::Unretained(this), &main_destroyed_event_)));
        impl_destroyed_->RequestCopyOfOutput(std::make_unique<
                                             viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(
                &LayerTreeHostCopyRequestTestLayerDestroyed::CopyOutputCallback,
                base::Unretained(this), &impl_destroyed_event_)));
        EXPECT_FALSE(main_destroyed_event_.IsSignaled());
        EXPECT_FALSE(impl_destroyed_event_.IsSignaled());

        // Destroy the main thread layer right away.
        main_destroyed_->RemoveFromParent();
        main_destroyed_.reset();

        // Prevent drawing so we can't make a copy of the impl_destroyed layer.
        layer_tree_host()->SetViewportRectAndScale(gfx::Rect(), 1.f,
                                                   GetCurrentLocalSurfaceId());
        break;
      case 2:
        // Flush the message loops.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        // There is no timing promise of when we'll get the main callback but if
        // we wait for it, we should receive it before we destroy the impl and
        // before the impl callback. The resulting bitmap will be empty because
        // we destroyed it in the first frame before it got a chance to draw.
        EXPECT_TRUE(
            main_destroyed_event_.TimedWait(TestTimeouts::action_timeout()));
        EXPECT_FALSE(impl_destroyed_event_.IsSignaled());

        // Destroy the impl thread layer.
        impl_destroyed_->RemoveFromParent();
        impl_destroyed_.reset();
        break;
      case 4:
        // Flush the message loops.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        // There is no timing promise of when we'll get the impl callback but if
        // we wait for it, we should receive it before the end of the test.
        // The resulting bitmap will be empty because we called
        // SetViewportRectAndScale() in the first frame before it got a chance
        // to draw.
        EXPECT_TRUE(
            impl_destroyed_event_.TimedWait(TestTimeouts::action_timeout()));
        EndTest();
        break;
    }
  }

  void CopyOutputCallback(base::WaitableEvent* event,
                          std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    event->Signal();
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  base::WaitableEvent main_destroyed_event_;
  scoped_refptr<FakePictureLayer> main_destroyed_;
  base::WaitableEvent impl_destroyed_event_;
  scoped_refptr<FakePictureLayer> impl_destroyed_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestLayerDestroyed,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestLayerDestroyed);

TEST_P(LayerTreeHostCopyRequestTestLayerDestroyed, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestInHiddenSubtree
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    grand_parent_layer_ = FakePictureLayer::Create(&client_);
    grand_parent_layer_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(grand_parent_layer_);

    // parent_layer_ owns a render surface.
    parent_layer_ = FakePictureLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetForceRenderSurfaceForTesting(true);
    grand_parent_layer_->AddChild(parent_layer_);

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(
            &LayerTreeHostCopyRequestTestInHiddenSubtree::CopyOutputCallback,
            base::Unretained(this))));
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();

    AddCopyRequest(copy_layer_.get());
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    ++callback_count_;
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString())
        << callback_count_;
    switch (callback_count_) {
      case 1:
        // Hide the copy request layer.
        grand_parent_layer_->SetHideLayerAndSubtree(false);
        parent_layer_->SetHideLayerAndSubtree(false);
        copy_layer_->SetHideLayerAndSubtree(true);
        AddCopyRequest(copy_layer_.get());
        break;
      case 2:
        // Hide the copy request layer's parent only.
        grand_parent_layer_->SetHideLayerAndSubtree(false);
        parent_layer_->SetHideLayerAndSubtree(true);
        copy_layer_->SetHideLayerAndSubtree(false);
        AddCopyRequest(copy_layer_.get());
        break;
      case 3:
        // Hide the copy request layer's grand parent only.
        grand_parent_layer_->SetHideLayerAndSubtree(true);
        parent_layer_->SetHideLayerAndSubtree(false);
        copy_layer_->SetHideLayerAndSubtree(false);
        AddCopyRequest(copy_layer_.get());
        break;
      case 4:
        // Hide the copy request layer's parent and grandparent.
        grand_parent_layer_->SetHideLayerAndSubtree(true);
        parent_layer_->SetHideLayerAndSubtree(true);
        copy_layer_->SetHideLayerAndSubtree(false);
        AddCopyRequest(copy_layer_.get());
        break;
      case 5:
        // Hide the copy request layer as well as its parent and grandparent.
        grand_parent_layer_->SetHideLayerAndSubtree(true);
        parent_layer_->SetHideLayerAndSubtree(true);
        copy_layer_->SetHideLayerAndSubtree(true);
        AddCopyRequest(copy_layer_.get());
        break;
      case 6:
        EndTest();
        break;
    }
  }

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> grand_parent_layer_;
  scoped_refptr<FakePictureLayer> parent_layer_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestInHiddenSubtree,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestInHiddenSubtree);

TEST_P(LayerTreeHostCopyRequestTestInHiddenSubtree, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    grand_parent_layer_ = FakePictureLayer::Create(&client_);
    grand_parent_layer_->SetBounds(gfx::Size(15, 15));
    grand_parent_layer_->SetHideLayerAndSubtree(true);
    root_->AddChild(grand_parent_layer_);

    // parent_layer_ owns a render surface.
    parent_layer_ = FakePictureLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetForceRenderSurfaceForTesting(true);
    grand_parent_layer_->AddChild(parent_layer_);

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    auto frame_sink = LayerTreeHostCopyRequestTest::CreateLayerTreeFrameSink(
        renderer_settings, refresh_rate, std::move(compositor_context_provider),
        std::move(worker_context_provider));
    frame_sink_ = frame_sink.get();
    return frame_sink;
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    copy_layer_->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(
            &LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest::
                CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString());
    EndTest();
  }

  void DisplayWillDrawAndSwapOnThread(
      bool will_draw_and_swap,
      const viz::AggregatedRenderPassList& render_passes) override {
    EXPECT_TRUE(will_draw_and_swap) << did_swap_;
    if (did_swap_) {
      // TODO(crbug.com/40447355): Ignore the extra frame that occurs due to
      // copy completion. This can be removed when the extra commit is removed.
      EXPECT_EQ(1u, render_passes.size());
      return;
    }

    EXPECT_EQ(2u, render_passes.size());
    // The root pass is the back of the list.
    copy_layer_render_pass_id = render_passes[0]->id;
    parent_render_pass_id = render_passes[1]->id;
  }

  void DisplayDidDrawAndSwapOnThread() override {
    viz::DirectRenderer* renderer =
        frame_sink_->display()->renderer_for_testing();

    // |parent| owns a surface, but it was hidden and not part of the copy
    // request so it should not allocate any resource.
    EXPECT_FALSE(
        renderer->HasAllocatedResourcesForTesting(parent_render_pass_id));

    // TODO(crbug.com/40447355): Ignore the extra frame that occurs due to copy
    // completion. This can be removed when the extra commit is removed.
    if (did_swap_) {
      EXPECT_FALSE(
          renderer->HasAllocatedResourcesForTesting(copy_layer_render_pass_id));
    } else {
      // |copy_layer| should have been rendered to a texture since it was needed
      // for a copy request.
      EXPECT_TRUE(
          renderer->HasAllocatedResourcesForTesting(copy_layer_render_pass_id));
    }

    did_swap_ = true;
  }

  void AfterTest() override { EXPECT_TRUE(did_swap_); }

  viz::AggregatedRenderPassId parent_render_pass_id;
  viz::AggregatedRenderPassId copy_layer_render_pass_id;
  raw_ptr<TestLayerTreeFrameSink, DanglingUntriaged> frame_sink_ = nullptr;
  bool did_swap_ = false;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> grand_parent_layer_;
  scoped_refptr<FakePictureLayer> parent_layer_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest,
    CombineWithCompositorModes(viz::GetGpuRendererTypes()),
    PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest);

TEST_P(LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestClippedOut
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    parent_layer_ = FakePictureLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetMasksToBounds(true);
    root_->AddChild(parent_layer_);

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetPosition(gfx::PointF(15.f, 15.f));
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    copy_layer_->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(
            &LayerTreeHostCopyRequestTestClippedOut::CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    // We should still get the content even if the copy requested layer was
    // completely clipped away.
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    EXPECT_EQ(gfx::Size(10, 10).ToString(), result->size().ToString());
    EndTest();
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> parent_layer_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestClippedOut,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestClippedOut);

TEST_P(LayerTreeHostCopyRequestTestClippedOut, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestScaledLayer
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(20, 20));

    gfx::Transform scale;
    scale.Scale(2, 2);

    copy_layer_ = Layer::Create();
    copy_layer_->SetBounds(gfx::Size(10, 10));
    copy_layer_->SetTransform(scale);
    root_->AddChild(copy_layer_);

    child_layer_ = FakePictureLayer::Create(&client_);
    child_layer_->SetBounds(gfx::Size(10, 10));
    copy_layer_->AddChild(child_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(
                &LayerTreeHostCopyRequestTestScaledLayer::CopyOutputCallback,
                base::Unretained(this)));
    request->set_area(gfx::Rect(5, 5));
    copy_layer_->RequestCopyOfOutput(std::move(request));
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    // The request area is expressed in layer space, but the result's size takes
    // into account the transform from layer space to surface space.
    EXPECT_EQ(gfx::Size(10, 10), result->size());
    EndTest();
  }

  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> copy_layer_;
  scoped_refptr<FakePictureLayer> child_layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestScaledLayer,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestScaledLayer);

TEST_P(LayerTreeHostCopyRequestTestScaledLayer, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostTestAsyncTwoReadbacksWithoutDraw
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(
            &LayerTreeHostTestAsyncTwoReadbacksWithoutDraw::CopyOutputCallback,
            base::Unretained(this))));
  }

  void BeginTest() override {
    saw_copy_request_ = false;
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();

    // Prevent drawing.
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(0, 0), 1.f,
                                               viz::LocalSurfaceId());

    AddCopyRequest(copy_layer_.get());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    if (impl->active_tree()->source_frame_number() == 0) {
      EXPECT_TRUE(impl->active_tree()->LayerById(copy_layer_->id()));
      saw_copy_request_ = true;
    }
  }

  void DidCommit() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      // Allow drawing.
      layer_tree_host()->SetViewportRectAndScale(
          gfx::Rect(root_->bounds()), 1.f, GetCurrentLocalSurfaceId());

      AddCopyRequest(copy_layer_.get());
    }
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());

    // The first frame can't be drawn.
    switch (callback_count_) {
      case 0:
        EXPECT_TRUE(result->IsEmpty());
        EXPECT_EQ(gfx::Size(), result->size());
        break;
      case 1:
        EXPECT_FALSE(result->IsEmpty());
        EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString());
        EndTest();
        break;
      default:
        NOTREACHED();
    }

    ++callback_count_;
  }

  void AfterTest() override { EXPECT_TRUE(saw_copy_request_); }

  bool saw_copy_request_;
  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTestAsyncTwoReadbacksWithoutDraw,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostTestAsyncTwoReadbacksWithoutDraw);

TEST_P(LayerTreeHostTestAsyncTwoReadbacksWithoutDraw, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestDeleteSharedImage
    : public LayerTreeHostCopyRequestTest {
 protected:
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayControllerOnThread() override {
    // In this implementation, none of the output surface has a real gpu thread,
    // and there is no overlay support.
    return nullptr;
  }
  std::unique_ptr<viz::SkiaOutputSurface> CreateSkiaOutputSurfaceOnThread(
      viz::DisplayCompositorMemoryAndTaskController*) override {
    display_context_provider_ = viz::TestContextProvider::Create();
    display_context_provider_->BindToCurrentSequence();
    return viz::FakeSkiaOutputSurface::Create3d(display_context_provider_);
  }

  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void ReceiveCopyRequestOutputAndCommit(
      std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->GetTaskRunnerProvider()->IsMainThread());
    EXPECT_EQ(gfx::Size(10, 10).ToString(), result->size().ToString());
    EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
    EXPECT_EQ(result->destination(),
              viz::CopyOutputResult::Destination::kNativeTextures);
    EXPECT_NE(result->GetTextureResult(), nullptr);

    // Save the result for later.
    EXPECT_FALSE(result_);
    result_ = std::move(result);

    // Post a commit to lose the output surface.
    layer_tree_host()->SetNeedsCommit();
  }

  void InsertCopyRequest() {
    copy_layer_->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputResult::Destination::kNativeTextures,
        base::BindOnce(&LayerTreeHostCopyRequestTestDeleteSharedImage::
                           ReceiveCopyRequestOutputAndCommit,
                       base::Unretained(this))));
  }

  void DestroyCopyResultAndCheckNumSharedImages() {
    EXPECT_TRUE(result_);
    result_.reset();

    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeHostCopyRequestTestDeleteSharedImage::
                           CheckNumSharedImagesAfterReadbackDestroyed,
                       base::Unretained(this)));
  }

  void CheckNumSharedImagesAfterReadbackDestroyed() {
    // After the copy we had |num_shared_images_after_readback_| many shared
    // images, but releasing the copy output request should cause the shared
    // image in the request to be destroyed by the compositor, so we should have
    // 1 less by now.
    EXPECT_EQ(num_shared_images_after_readback_ - 1,
              display_context_provider_->SharedImageInterface()
                  ->shared_image_count());

    // Drop the reference to the context provider on the compositor thread.
    display_context_provider_.reset();
    EndTest();
  }

  void DisplayDidDrawAndSwapOnThread() override {
    auto* sii = display_context_provider_->SharedImageInterface();
    switch (num_swaps_++) {
      case 0:
        // The layers have been drawn, so any textures required for drawing have
        // been allocated.
        EXPECT_FALSE(result_);
        num_shared_images_without_readback_ = sii->shared_image_count();

        // Request a copy of the layer. This will use another texture.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostCopyRequestTestDeleteSharedImage::
                               InsertCopyRequest,
                           base::Unretained(this)));
        break;
      case 1:
        // Copy requests cause a followup commit and draw without the separate
        // RenderPass required. This changes the number of active textures. So
        // wait for that draw before counting textures and proceeding.
        break;
      case 2:
        // We did a readback, so there will be a readback texture around now.
        num_shared_images_after_readback_ = sii->shared_image_count();
        EXPECT_LT(num_shared_images_without_readback_,
                  num_shared_images_after_readback_);

        // Now destroy the CopyOutputResult, releasing the texture inside back
        // to the compositor. Then check the resulting number of allocated
        // textures.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostCopyRequestTestDeleteSharedImage::
                               DestroyCopyResultAndCheckNumSharedImages,
                           base::Unretained(this)));
        break;
    }
  }

  scoped_refptr<viz::TestContextProvider> display_context_provider_;
  int num_swaps_ = 0;
  size_t num_shared_images_without_readback_ = 0;
  size_t num_shared_images_after_readback_ = 0;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
  std::unique_ptr<viz::CopyOutputResult> result_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestDeleteSharedImage,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestDeleteSharedImage);

TEST_P(LayerTreeHostCopyRequestTestDeleteSharedImage, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestCountSharedImages
    : public LayerTreeHostCopyRequestTest {
 protected:
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayControllerOnThread() override {
    // In this implementation, none of the output surface has a real gpu thread,
    // and there is no overlay support.
    return nullptr;
  }
  std::unique_ptr<viz::SkiaOutputSurface> CreateSkiaOutputSurfaceOnThread(
      viz::DisplayCompositorMemoryAndTaskController*) override {
    display_context_provider_ = viz::TestContextProvider::Create();
    display_context_provider_->BindToCurrentSequence();
    return viz::FakeSkiaOutputSurface::Create3d(display_context_provider_);
  }

  void SetupTree() override {
    // The layers in this test have solid color content, so they don't
    // actually allocate any textures, making counting easier.

    root_ = FakePictureLayer::Create(&root_client_);
    root_->SetBounds(gfx::Size(20, 20));
    root_client_.set_bounds(root_->bounds());

    copy_layer_ = FakePictureLayer::Create(&copy_client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    copy_client_.set_bounds(copy_layer_->bounds());
    PaintFlags flags;
    flags.setColor(SK_ColorRED);
    // Ensure the layer isn't completely transparent so the RenderPass isn't
    // optimized away.
    copy_client_.add_draw_rect(gfx::Rect(0, 0, 10, 10), flags);
    // Doing a copy makes the layer have a render surface which can cause
    // texture allocations. So get those allocations out of the way in the
    // first frame by forcing it to have a render surface.
    copy_layer_->SetForceRenderSurfaceForTesting(true);
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  virtual void RequestCopy(Layer* layer) = 0;

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // The layers have been pushed to the impl side and drawn. Any textures
        // that are created in that process will have been allocated.
        RequestCopy(copy_layer_.get());
        break;
    }
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    // Since this test counts shared images and SkiaRenderer uses shared images
    // for render passes, we need render pass allocation to be stable.
    auto settings = renderer_settings;
    settings.disable_render_pass_bypassing = true;
    auto frame_sink = LayerTreeHostCopyRequestTest::CreateLayerTreeFrameSink(
        settings, refresh_rate, std::move(compositor_context_provider),
        std::move(worker_context_provider));
    return frame_sink;
  }

  void DisplayDidDrawAndSwapOnThread() override {
    auto* sii = display_context_provider_->SharedImageInterface();
    switch (num_swaps_++) {
      case 0:
        // The first frame has been drawn without readback, so result shared
        // images not have been allocated.
        num_shared_images_without_readback_ = sii->shared_image_count();
        break;
      case 1:
        // We did a readback, so there will be a result shared image around now.
        num_shared_images_with_readback_ = sii->shared_image_count();

        // End the test after main thread has a chance to hear about the
        // readback.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &LayerTreeHostCopyRequestTestCountSharedImages::DoEndTest,
                base::Unretained(this)));
        break;
    }
  }

  virtual void DoEndTest() {
    // Drop the reference to the context provider on the main thread. If the
    // reference is dropped during the test destructor, then there will be a
    // DCHECK in ~TestContextProvider() for some cases.
    display_context_provider_.reset();
    EndTest();
  }

  scoped_refptr<viz::TestContextProvider> display_context_provider_;
  int num_swaps_ = 0;
  size_t num_shared_images_without_readback_ = 0;
  size_t num_shared_images_with_readback_ = 0;
  FakeContentLayerClient root_client_;
  FakeContentLayerClient copy_client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

class LayerTreeHostCopyRequestTestCreatesSharedImage
    : public LayerTreeHostCopyRequestTestCountSharedImages {
 protected:
  void RequestCopy(Layer* layer) override {
    // Request a normal texture copy. This should create a new shared image.
    copy_layer_->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputResult::Destination::kNativeTextures,
        base::BindOnce(
            &LayerTreeHostCopyRequestTestCreatesSharedImage::CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
    EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
    EXPECT_EQ(result->destination(),
              viz::CopyOutputResult::Destination::kNativeTextures);
    ASSERT_NE(nullptr, result->GetTextureResult());
    release_ = result->TakeTextureOwnership();
    EXPECT_EQ(1u, release_.size());
  }

  void AfterTest() override {
    for (auto& release : release_) {
      std::move(release).Run(gpu::SyncToken(), false);
    }

    // Except the copy to have made a new shared image.
    EXPECT_EQ(num_shared_images_without_readback_ + 1,
              num_shared_images_with_readback_);
  }

  viz::CopyOutputResult::ReleaseCallbacks release_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestCreatesSharedImage,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestCreatesSharedImage);

TEST_P(LayerTreeHostCopyRequestTestCreatesSharedImage, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestDestroyBeforeCopy
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LayerTreeHostCopyRequestTestDestroyBeforeCopy::DidActivate,
            base::Unretained(this)));
  }

  void DidActivate() {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1: {
        EXPECT_EQ(0, callback_count_);
        // Put a copy request on the layer, but then don't allow any
        // drawing to take place.
        std::unique_ptr<viz::CopyOutputRequest> request =
            std::make_unique<viz::CopyOutputRequest>(
                viz::CopyOutputRequest::ResultFormat::RGBA,
                viz::CopyOutputResult::Destination::kNativeTextures,
                base::BindOnce(&LayerTreeHostCopyRequestTestDestroyBeforeCopy::
                                   CopyOutputCallback,
                               base::Unretained(this)));
        copy_layer_->RequestCopyOfOutput(std::move(request));

        // Stop drawing.
        layer_tree_host()->SetViewportRectAndScale(gfx::Rect(), 1.f,
                                                   GetCurrentLocalSurfaceId());
        break;
      }
      case 2:
        EXPECT_EQ(0, callback_count_);
        // Remove the copy layer before we were able to draw.
        copy_layer_->RemoveFromParent();
        break;
      case 3:
        EXPECT_EQ(1, callback_count_);
        // Allow us to draw now.
        layer_tree_host()->SetViewportRectAndScale(
            gfx::Rect(layer_tree_host()->root_layer()->bounds()), 1.f,
            GetCurrentLocalSurfaceId());
        break;
      case 4:
        EXPECT_EQ(1, callback_count_);
        // We should not have crashed.
        EndTest();
    }
  }

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestDestroyBeforeCopy,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestDestroyBeforeCopy);

TEST_P(LayerTreeHostCopyRequestTestDestroyBeforeCopy, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestShutdownBeforeCopy
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LayerTreeHostCopyRequestTestShutdownBeforeCopy::DidActivate,
            base::Unretained(this)));
  }

  void DidActivate() {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1: {
        EXPECT_EQ(0, callback_count_);
        // Put a copy request on the layer, but then don't allow any
        // drawing to take place.
        std::unique_ptr<viz::CopyOutputRequest> request =
            std::make_unique<viz::CopyOutputRequest>(
                viz::CopyOutputRequest::ResultFormat::RGBA,
                viz::CopyOutputResult::Destination::kNativeTextures,
                base::BindOnce(&LayerTreeHostCopyRequestTestShutdownBeforeCopy::
                                   CopyOutputCallback,
                               base::Unretained(this)));
        copy_layer_->RequestCopyOfOutput(std::move(request));

        layer_tree_host()->SetViewportRectAndScale(gfx::Rect(), 1.f,
                                                   GetCurrentLocalSurfaceId());
        break;
      }
      case 2:
        DestroyLayerTreeHost();
        // End the test after the copy result has had a chance to get back to
        // the main thread.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &LayerTreeHostCopyRequestTestShutdownBeforeCopy::EndTest,
                base::Unretained(this)));
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, callback_count_); }

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostCopyRequestTestShutdownBeforeCopy,
                         CombineWithCompositorModes(viz::GetGpuRendererTypes()),
                         PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestShutdownBeforeCopy);

TEST_P(LayerTreeHostCopyRequestTestShutdownBeforeCopy, Test) {
  RunTest(compositor_mode());
}

class LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    scoped_refptr<FakePictureLayer> root = FakePictureLayer::Create(&client_);
    root->SetBounds(gfx::Size(20, 20));

    child_ = FakePictureLayer::Create(&client_);
    child_->SetBounds(gfx::Size(10, 10));
    root->AddChild(child_);
    child_->SetHideLayerAndSubtree(true);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override {
    num_draws_ = 0;
    copy_happened_ = false;
    draw_happened_ = false;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    // Send a copy request after the first commit.
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      child_->RequestCopyOfOutput(std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              &LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest::
                  CopyOutputCallback,
              base::Unretained(this))));
    }
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    LayerImpl* root = host_impl->active_tree()->root_layer();
    LayerImpl* child = host_impl->active_tree()->LayerById(child_->id());

    bool saw_root = false;
    bool saw_child = false;
    for (EffectTreeLayerListIterator it(host_impl->active_tree());
         it.state() != EffectTreeLayerListIterator::State::kEnd; ++it) {
      if (it.state() == EffectTreeLayerListIterator::State::kLayer) {
        if (it.current_layer() == root)
          saw_root = true;
        else if (it.current_layer() == child)
          saw_child = true;
        else
          NOTREACHED();
      }
    }

    ++num_draws_;
    // The first draw has no copy request. The 2nd draw has a copy request, the
    // 3rd should not again.
    switch (num_draws_) {
      case 1:
        // Only the root layer draws, the child is hidden.
        EXPECT_TRUE(saw_root);
        EXPECT_FALSE(saw_child);
        break;
      case 2:
        // Copy happening here, the child will draw.
        EXPECT_TRUE(saw_root);
        EXPECT_TRUE(saw_child);
        // Make another draw happen after doing the copy request.
        host_impl->SetViewportDamage(gfx::Rect(1, 1));
        host_impl->SetNeedsRedraw();
        break;
      case 3:
        // If LayerTreeHostImpl does the wrong thing, it will try to draw the
        // layer which had a copy request. But only the root should draw.
        EXPECT_TRUE(saw_root);
        EXPECT_FALSE(saw_child);

        // End the test! Don't race with copy request callbacks, so post the end
        // to the main thread.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest::
                    TryEndTest,
                base::Unretained(this), WhatHappened::DRAW));
        break;
    }
    return draw_result;
  }

  void CopyOutputCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_FALSE(TestEnded());
    TryEndTest(WhatHappened::COPY);
  }

  enum class WhatHappened {
    DRAW,
    COPY,
  };

  void TryEndTest(WhatHappened what) {
    switch (what) {
      case WhatHappened::DRAW:
        draw_happened_ = true;
        break;
      case WhatHappened::COPY:
        copy_happened_ = true;
        break;
    }
    if (draw_happened_ && copy_happened_)
      EndTest();
  }

  scoped_refptr<FakePictureLayer> child_;
  FakeContentLayerClient client_;
  int num_draws_;
  bool copy_happened_;
  bool draw_happened_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest,
    CombineWithCompositorModes(viz::GetGpuRendererTypes()),
    PrintTupleToStringParamName());

// viz::GetGpuRendererTypes() is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest);

TEST_P(LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest, Test) {
  RunTest(compositor_mode());
}

}  // namespace
}  // namespace cc
