// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/base/completion_event.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using ::testing::Mock;

using ScrollThread = cc::InputHandler::ScrollThread;

namespace cc {
namespace {

std::unique_ptr<ScrollState> BeginState(const gfx::Point& point,
                                        const gfx::Vector2dF& delta_hint) {
  ScrollStateData scroll_state_data;
  scroll_state_data.is_beginning = true;
  scroll_state_data.position_x = point.x();
  scroll_state_data.position_y = point.y();
  scroll_state_data.delta_x_hint = delta_hint.x();
  scroll_state_data.delta_y_hint = delta_hint.y();
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));
  return scroll_state;
}

std::unique_ptr<ScrollState> UpdateState(const gfx::Point& point,
                                         const gfx::Vector2dF& delta) {
  ScrollStateData scroll_state_data;
  scroll_state_data.delta_x = delta.x();
  scroll_state_data.delta_y = delta.y();
  scroll_state_data.position_x = point.x();
  scroll_state_data.position_y = point.y();
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));
  return scroll_state;
}

class LayerTreeHostScrollTest : public LayerTreeTest, public ScrollCallbacks {
 protected:
  LayerTreeHostScrollTest() { SetUseLayerLists(); }

  void SetupTree() override {
    LayerTreeTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();

    // Create an effective max_scroll_offset of (100, 100).
    gfx::Size scroll_layer_bounds(root_layer->bounds().width() + 100,
                                  root_layer->bounds().height() + 100);

    SetupViewport(root_layer, root_layer->bounds(), scroll_layer_bounds);
    layer_tree_host()->property_trees()->scroll_tree.SetScrollCallbacks(
        weak_ptr_factory_.GetWeakPtr());
  }

  // ScrollCallbacks
  void DidScroll(ElementId element_id,
                 const gfx::ScrollOffset& scroll_offset,
                 const base::Optional<TargetSnapAreaElementIds>&
                     snap_target_ids) override {
    // Simulates cc client (e.g Blink) behavior when handling impl-side scrolls.
    SetScrollOffsetFromImplSide(layer_tree_host()->LayerByElementId(element_id),
                                scroll_offset);

    if (element_id ==
        layer_tree_host()->OuterViewportScrollLayerForTesting()->element_id()) {
      DidScrollOuterViewport(scroll_offset);
    }
    if (snap_target_ids.has_value()) {
      ScrollNode* scroller_node =
          layer_tree_host()
              ->property_trees()
              ->scroll_tree.FindNodeFromElementId(element_id);
      scroller_node->snap_container_data.value().SetTargetSnapAreaElementIds(
          snap_target_ids.value());
    }
  }
  void DidChangeScrollbarsHidden(ElementId, bool) override {}

  virtual void DidScrollOuterViewport(const gfx::ScrollOffset& scroll_offset) {
    num_outer_viewport_scrolls_++;
  }

  int num_outer_viewport_scrolls_ = 0;

 private:
  base::WeakPtrFactory<LayerTreeHostScrollTest> weak_ptr_factory_{this};
};

class LayerTreeHostScrollTestScrollSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSimple()
      : initial_scroll_(10, 20), second_scroll_(40, 5), scroll_amount_(2, -1) {}

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    if (!layer_tree_host()->SourceFrameNumber()) {
      EXPECT_VECTOR_EQ(initial_scroll_,
                       GetTransformNode(scroll_layer)->scroll_offset);
    } else {
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_, scroll_amount_),
          GetTransformNode(scroll_layer)->scroll_offset);

      // Pretend like Javascript updated the scroll position itself.
      SetScrollOffset(scroll_layer, second_scroll_);
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();
    EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));

    scroll_layer->SetBounds(
        gfx::Size(root->bounds().width() + 100, root->bounds().height() + 100));
    scroll_layer->ScrollBy(scroll_amount_);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(second_scroll_, ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        EndTest();
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, num_outer_viewport_scrolls_); }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::ScrollOffset second_scroll_;
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSimple);

class LayerTreeHostScrollTestScrollMultipleRedraw
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMultipleRedraw()
      : initial_scroll_(40, 10), scroll_amount_(-3, 17) {}

  void BeginTest() override {
    scroll_layer_ = layer_tree_host()->OuterViewportScrollLayerForTesting();
    SetScrollOffset(scroll_layer_.get(), initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_,
                         CurrentScrollOffset(scroll_layer_.get()));
        break;
      case 1:
      case 2:
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(
                             initial_scroll_, scroll_amount_ + scroll_amount_),
                         CurrentScrollOffset(scroll_layer_.get()));
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer =
        impl->active_tree()->LayerById(scroll_layer_->id());
    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumberForTesting() == 1) {
      // First draw after first commit.
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
      scroll_layer->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));

      EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
      PostSetNeedsRedrawToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumberForTesting() == 2) {
      // Second draw after first commit.
      EXPECT_VECTOR_EQ(ScrollDelta(scroll_layer), scroll_amount_);
      scroll_layer->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(scroll_amount_ + scroll_amount_,
                       ScrollDelta(scroll_layer));

      EXPECT_VECTOR_EQ(initial_scroll_,
                       CurrentScrollOffset(scroll_layer_.get()));
      PostSetNeedsCommitToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Third or later draw after second commit.
      EXPECT_GE(impl->SourceAnimationFrameNumberForTesting(), 3u);
      EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(
                           initial_scroll_, scroll_amount_ + scroll_amount_),
                       CurrentScrollOffset(scroll_layer_.get()));
      EndTest();
    }
  }

  void AfterTest() override { EXPECT_EQ(1, num_outer_viewport_scrolls_); }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF scroll_amount_;
  scoped_refptr<Layer> scroll_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMultipleRedraw);

class LayerTreeHostScrollTestScrollAbortedCommit
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollAbortedCommit()
      : initial_scroll_(50, 60),
        impl_scroll_(-3, 2),
        second_main_scroll_(14, -3),
        impl_scale_(2.f),
        num_will_begin_main_frames_(0),
        num_did_begin_main_frames_(0),
        num_will_commits_(0),
        num_did_commits_(0),
        num_impl_commits_(0) {}

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    gfx::Size scroll_layer_bounds(200, 200);
    layer_tree_host()->OuterViewportScrollLayerForTesting()->SetBounds(
        scroll_layer_bounds);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void WillBeginMainFrame() override {
    num_will_begin_main_frames_++;
    Layer* root_scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    switch (num_will_begin_main_frames_) {
      case 1:
        // This will not be aborted because of the initial prop changes.
        EXPECT_EQ(0, num_outer_viewport_scrolls_);
        EXPECT_EQ(0, layer_tree_host()->SourceFrameNumber());
        EXPECT_VECTOR_EQ(initial_scroll_,
                         CurrentScrollOffset(root_scroll_layer));
        EXPECT_EQ(1.f, layer_tree_host()->page_scale_factor());
        break;
      case 2:
        // This commit will be aborted, and another commit will be
        // initiated from the redraw.
        EXPECT_EQ(1, num_outer_viewport_scrolls_);
        EXPECT_EQ(1, layer_tree_host()->SourceFrameNumber());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, impl_scroll_),
            CurrentScrollOffset(root_scroll_layer));
        EXPECT_EQ(impl_scale_, layer_tree_host()->page_scale_factor());
        PostSetNeedsRedrawToMainThread();
        break;
      case 3:
        // This commit will not be aborted because of the scroll change.
        EXPECT_EQ(2, num_outer_viewport_scrolls_);
        // The source frame number still increases even with the abort.
        EXPECT_EQ(2, layer_tree_host()->SourceFrameNumber());
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(
                             initial_scroll_, impl_scroll_ + impl_scroll_),
                         CurrentScrollOffset(root_scroll_layer));
        EXPECT_EQ(impl_scale_ * impl_scale_,
                  layer_tree_host()->page_scale_factor());
        SetScrollOffset(
            root_scroll_layer,
            gfx::ScrollOffsetWithDelta(CurrentScrollOffset(root_scroll_layer),
                                       second_main_scroll_));
        break;
      case 4:
        // This commit will also be aborted.
        EXPECT_EQ(3, num_outer_viewport_scrolls_);
        EXPECT_EQ(3, layer_tree_host()->SourceFrameNumber());
        gfx::Vector2dF delta =
            impl_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_;
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                         CurrentScrollOffset(root_scroll_layer));

        // End the test by drawing to verify this commit is also aborted.
        PostSetNeedsRedrawToMainThread();
        break;
    }
  }

  void DidBeginMainFrame() override { num_did_begin_main_frames_++; }

  void WillCommit() override { num_will_commits_++; }

  void DidCommit() override { num_did_commits_++; }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    num_impl_commits_++;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root_scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();

    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumberForTesting() == 1) {
      // First draw
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
      root_scroll_layer->ScrollBy(impl_scroll_);
      EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
      EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(root_scroll_layer));

      EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(1.f, impl->active_tree()->current_page_scale_factor());
      impl->active_tree()->SetPageScaleOnActiveTree(impl_scale_);
      EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_, impl->active_tree()->current_page_scale_factor());

      // To simplify the testing flow, don't redraw here, just commit.
      impl->SetNeedsCommit();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumberForTesting() == 2) {
      // Test a second draw after an aborted commit.
      // The scroll/scale values should be baked into the offset/scale factor
      // since the main thread consumed but aborted the begin frame.
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
      root_scroll_layer->ScrollBy(impl_scroll_);
      EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_, impl_scroll_),
          ScrollOffsetBase(root_scroll_layer));

      EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_, impl->active_tree()->current_page_scale_factor());
      impl->active_tree()->SetPageScaleOnActiveTree(impl_scale_ * impl_scale_);
      EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_ * impl_scale_,
                impl->active_tree()->current_page_scale_factor());

      impl->SetNeedsCommit();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Commit for source frame 1 is aborted.
      NOTREACHED();
    } else if (impl->active_tree()->source_frame_number() == 2 &&
               impl->SourceAnimationFrameNumberForTesting() == 3) {
      // Third draw after the second full commit.
      EXPECT_EQ(ScrollDelta(root_scroll_layer), gfx::ScrollOffset());
      root_scroll_layer->ScrollBy(impl_scroll_);
      impl->SetNeedsCommit();
      EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
      gfx::Vector2dF delta = impl_scroll_ + impl_scroll_ + second_main_scroll_;
      EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                       ScrollOffsetBase(root_scroll_layer));
    } else if (impl->active_tree()->source_frame_number() == 2 &&
               impl->SourceAnimationFrameNumberForTesting() == 4) {
      // Final draw after the second aborted commit.
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
      gfx::Vector2dF delta =
          impl_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_;
      EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                       ScrollOffsetBase(root_scroll_layer));
      EndTest();
    } else {
      // Commit for source frame 3 is aborted.
      NOTREACHED();
    }
  }

  void AfterTest() override {
    EXPECT_EQ(3, num_outer_viewport_scrolls_);
    // Verify that the embedder sees aborted commits as real commits.
    EXPECT_EQ(4, num_will_begin_main_frames_);
    EXPECT_EQ(4, num_did_begin_main_frames_);
    EXPECT_EQ(4, num_will_commits_);
    EXPECT_EQ(4, num_did_commits_);
    // ...but the compositor thread only sees two real ones.
    EXPECT_EQ(2, num_impl_commits_);
  }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_scroll_;
  gfx::Vector2dF second_main_scroll_;
  float impl_scale_;
  int num_will_begin_main_frames_;
  int num_did_begin_main_frames_;
  int num_will_commits_;
  int num_did_commits_;
  int num_impl_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollAbortedCommit);

class LayerTreeHostScrollTestFractionalScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestFractionalScroll() : scroll_amount_(1.75, 0) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();

    // Check that a fractional scroll delta is correctly accumulated over
    // multiple commits.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(gfx::Vector2d(0, 0), ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(gfx::Vector2d(0, 0), ScrollDelta(scroll_layer));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(gfx::ToRoundedVector2d(scroll_amount_),
                         ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(
            scroll_amount_ - gfx::ToRoundedVector2d(scroll_amount_),
            ScrollDelta(scroll_layer));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            gfx::ToRoundedVector2d(scroll_amount_ + scroll_amount_),
            ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(
            scroll_amount_ + scroll_amount_ -
                gfx::ToRoundedVector2d(scroll_amount_ + scroll_amount_),
            ScrollDelta(scroll_layer));
        EndTest();
        break;
    }
    scroll_layer->ScrollBy(scroll_amount_);
  }

 private:
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestFractionalScroll);

class LayerTreeHostScrollTestScrollSnapping : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSnapping() : scroll_amount_(1.75, 0) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    scoped_refptr<Layer> container = Layer::Create();
    container->SetBounds(gfx::Size(100, 100));
    CopyProperties(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                   container.get());
    CreateTransformNode(container.get()).post_translation =
        gfx::Vector2dF(0.25, 0);
    CreateEffectNode(container.get()).render_surface_reason =
        RenderSurfaceReason::kTest;
    layer_tree_host()->root_layer()->AddChild(container);

    scroll_layer_ = Layer::Create();
    scroll_layer_->SetBounds(gfx::Size(200, 200));
    scroll_layer_->SetIsDrawable(true);
    scroll_layer_->SetElementId(
        LayerIdToElementIdForTesting(scroll_layer_->id()));
    CopyProperties(container.get(), scroll_layer_.get());
    CreateTransformNode(scroll_layer_.get());
    CreateScrollNode(scroll_layer_.get(), gfx::Size(100, 100));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.1f, 100.f);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer =
        impl->active_tree()->LayerById(scroll_layer_->id());

    gfx::Transform translate;

    // Check that screen space transform of the scrollable layer is correctly
    // snapped to integers.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_EQ(gfx::Transform(),
                  scroll_layer->draw_properties().screen_space_transform);
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        translate.Translate(-2, 0);
        EXPECT_EQ(translate,
                  scroll_layer->draw_properties().screen_space_transform);
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        translate.Translate(-3, 0);
        EXPECT_EQ(translate,
                  scroll_layer->draw_properties().screen_space_transform);
        EndTest();
        break;
    }
    scroll_layer->ScrollBy(scroll_amount_);
  }

 private:
  scoped_refptr<Layer> scroll_layer_;
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSnapping);

class LayerTreeHostScrollTestCaseWithChild : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestCaseWithChild()
      : initial_offset_(10, 20),
        javascript_scroll_(40, 5),
        scroll_amount_(2, -1) {}

  void SetupTree() override {
    SetInitialDeviceScaleFactor(device_scale_factor_);
    SetInitialRootBounds(gfx::Size(10, 10));
    LayerTreeHostScrollTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    Layer* root_scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();

    child_layer_ = Layer::Create();
    child_layer_->SetElementId(
        LayerIdToElementIdForTesting(child_layer_->id()));
    child_layer_->SetBounds(gfx::Size(110, 110));

    gfx::Vector2dF child_layer_offset;
    // Adjust the child layer horizontally so that scrolls will never hit it.
    if (scroll_child_layer_) {
      // Scrolls on the child layer will happen at 5, 5. If they are treated
      // like device pixels, and device scale factor is 2, then they will
      // be considered at 2.5, 2.5 in logical pixels, and will miss this layer.
      child_layer_offset = gfx::Vector2dF(5.f, 5.f);
    } else {
      child_layer_offset = gfx::Vector2dF(60.f, 5.f);
    }

    child_layer_->SetIsDrawable(true);
    child_layer_->SetHitTestable(true);
    child_layer_->SetElementId(
        LayerIdToElementIdForTesting(child_layer_->id()));
    child_layer_->SetBounds(root_scroll_layer->bounds());
    root_layer->AddChild(child_layer_);

    CopyProperties(root_scroll_layer, child_layer_.get());
    CreateTransformNode(child_layer_.get()).post_translation =
        child_layer_offset;
    CreateScrollNode(child_layer_.get(), root_layer->bounds());

    if (scroll_child_layer_) {
      expected_scroll_layer_ = child_layer_.get();
      expected_no_scroll_layer_ = root_scroll_layer;
    } else {
      expected_scroll_layer_ = root_scroll_layer;
      expected_no_scroll_layer_ = child_layer_.get();
    }

    SetScrollOffset(expected_scroll_layer_, initial_offset_);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void WillCommit() override {
    // Keep the test committing (otherwise the early out for no update
    // will stall the test).
    if (layer_tree_host()->SourceFrameNumber() < 2) {
      layer_tree_host()->SetNeedsCommit();
    }
  }

  void DidScroll(ElementId element_id,
                 const gfx::ScrollOffset& offset,
                 const base::Optional<TargetSnapAreaElementIds>&
                     snap_target_ids) override {
    LayerTreeHostScrollTest::DidScroll(element_id, offset, snap_target_ids);
    if (element_id == expected_scroll_layer_->element_id()) {
      final_scroll_offset_ = CurrentScrollOffset(expected_scroll_layer_);
      EXPECT_EQ(offset, final_scroll_offset_);
      EXPECT_EQ(element_id, expected_scroll_layer_->element_id());
    } else {
      EXPECT_TRUE(offset.IsZero());
    }
  }

  void UpdateLayerTreeHost() override {
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     CurrentScrollOffset(expected_no_scroll_layer_));

    switch (layer_tree_host()->SourceFrameNumber()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_offset_,
                         CurrentScrollOffset(expected_scroll_layer_));
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_offset_, scroll_amount_),
            CurrentScrollOffset(expected_scroll_layer_));

        // Pretend like Javascript updated the scroll position itself.
        SetScrollOffset(expected_scroll_layer_, javascript_scroll_);
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(javascript_scroll_, scroll_amount_),
            CurrentScrollOffset(expected_scroll_layer_));
        break;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* inner_scroll =
        impl->active_tree()->InnerViewportScrollLayerForTesting();
    LayerImpl* root_scroll_layer_impl =
        impl->active_tree()->OuterViewportScrollLayerForTesting();
    LayerImpl* child_layer_impl =
        root_scroll_layer_impl->layer_tree_impl()->LayerById(
            child_layer_->id());

    LayerImpl* expected_scroll_layer_impl = nullptr;
    LayerImpl* expected_no_scroll_layer_impl = nullptr;
    if (scroll_child_layer_) {
      expected_scroll_layer_impl = child_layer_impl;
      expected_no_scroll_layer_impl = root_scroll_layer_impl;
    } else {
      expected_scroll_layer_impl = root_scroll_layer_impl;
      expected_no_scroll_layer_impl = child_layer_impl;
    }

    EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(inner_scroll));
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     ScrollDelta(expected_no_scroll_layer_impl));

    // Ensure device scale factor matches the active tree.
    EXPECT_EQ(device_scale_factor_, impl->active_tree()->device_scale_factor());
    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        // GESTURE scroll on impl thread. Also tests that the last scrolled
        // layer id is stored even after the scrolling ends.
        gfx::Point scroll_point = gfx::ToCeiledPoint(
            gfx::PointF(-0.5f, -0.5f) +
            GetTransformNode(expected_scroll_layer_impl)->post_translation);
        InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
            BeginState(scroll_point, scroll_amount_).get(),
            ui::ScrollInputType::kTouchscreen);
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        impl->GetInputHandler().ScrollUpdate(
            UpdateState(gfx::Point(), scroll_amount_).get());
        auto* scrolling_node = impl->CurrentlyScrollingNode();
        CHECK(scrolling_node);
        impl->GetInputHandler().ScrollEnd();
        CHECK(!impl->CurrentlyScrollingNode());
        EXPECT_EQ(scrolling_node->id,
                  impl->active_tree()->LastScrolledScrollNodeIndex());

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(initial_offset_,
                         ScrollOffsetBase(expected_scroll_layer_impl));
        EXPECT_VECTOR_EQ(scroll_amount_,
                         ScrollDelta(expected_scroll_layer_impl));
        break;
      }
      case 1: {
        // WHEEL scroll on impl thread.
        gfx::Point scroll_point = gfx::ToCeiledPoint(
            gfx::PointF(0.5f, 0.5f) +
            GetTransformNode(expected_scroll_layer_impl)->post_translation);
        InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
            BeginState(scroll_point, scroll_amount_).get(),
            ui::ScrollInputType::kWheel);
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        impl->GetInputHandler().ScrollUpdate(
            UpdateState(gfx::Point(), scroll_amount_).get());
        impl->GetInputHandler().ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(javascript_scroll_,
                         ScrollOffsetBase(expected_scroll_layer_impl));
        EXPECT_VECTOR_EQ(scroll_amount_,
                         ScrollDelta(expected_scroll_layer_impl));
        break;
      }
      case 2:

        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(javascript_scroll_, scroll_amount_),
            ScrollOffsetBase(expected_scroll_layer_impl));
        EXPECT_VECTOR_EQ(gfx::Vector2d(),
                         ScrollDelta(expected_scroll_layer_impl));

        EndTest();
        break;
    }
  }

  void AfterTest() override {
    EXPECT_EQ(scroll_child_layer_ ? 0 : 2, num_outer_viewport_scrolls_);
    EXPECT_VECTOR_EQ(
        gfx::ScrollOffsetWithDelta(javascript_scroll_, scroll_amount_),
        final_scroll_offset_);
  }

 protected:
  float device_scale_factor_;
  bool scroll_child_layer_;

  gfx::ScrollOffset initial_offset_;
  gfx::ScrollOffset javascript_scroll_;
  gfx::Vector2d scroll_amount_;
  gfx::ScrollOffset final_scroll_offset_;

  scoped_refptr<Layer> child_layer_;
  Layer* expected_scroll_layer_;
  Layer* expected_no_scroll_layer_;
};

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor1_ScrollChild) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor15_ScrollChild) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild, DeviceScaleFactor2_ScrollChild) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED);
}

class LayerTreeHostScrollTestSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestSimple()
      : initial_scroll_(10, 20),
        main_thread_scroll_(40, 5),
        impl_thread_scroll1_(2, -1),
        impl_thread_scroll2_(-3, 10) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    if (!layer_tree_host()->SourceFrameNumber()) {
      EXPECT_VECTOR_EQ(initial_scroll_, CurrentScrollOffset(scroll_layer));
    } else {
      EXPECT_VECTOR_EQ(
          CurrentScrollOffset(scroll_layer),
          gfx::ScrollOffsetWithDelta(initial_scroll_, impl_thread_scroll1_));

      // Pretend like Javascript updated the scroll position itself with a
      // change of main_thread_scroll.
      SetScrollOffset(
          scroll_layer,
          gfx::ScrollOffsetWithDelta(
              initial_scroll_, main_thread_scroll_ + impl_thread_scroll1_));
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    // We force a second draw here of the first commit before activating
    // the second commit.
    if (impl->active_tree()->source_frame_number() == 0)
      impl->SetNeedsRedraw();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (impl->pending_tree())
      impl->SetNeedsRedraw();

    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();
    LayerImpl* pending_root =
        impl->active_tree()->FindPendingTreeLayerById(root->id());

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        if (!impl->pending_tree()) {
          impl->BlockNotifyReadyToActivateForTesting(true);
          EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
          scroll_layer->ScrollBy(impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
          EXPECT_VECTOR_EQ(impl_thread_scroll1_, ScrollDelta(scroll_layer));
          PostSetNeedsCommitToMainThread();

          // CommitCompleteOnThread will trigger this function again
          // and cause us to take the else clause.
        } else {
          impl->BlockNotifyReadyToActivateForTesting(false);
          ASSERT_TRUE(pending_root);
          EXPECT_EQ(impl->pending_tree()->source_frame_number(), 1);

          scroll_layer->ScrollBy(impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
          EXPECT_VECTOR_EQ(impl_thread_scroll1_ + impl_thread_scroll2_,
                           ScrollDelta(scroll_layer));

          LayerImpl* pending_scroll_layer =
              impl->pending_tree()->OuterViewportScrollLayerForTesting();
          EXPECT_VECTOR_EQ(
              gfx::ScrollOffsetWithDelta(
                  initial_scroll_, main_thread_scroll_ + impl_thread_scroll1_),
              ScrollOffsetBase(pending_scroll_layer));
          EXPECT_VECTOR_EQ(impl_thread_scroll2_,
                           ScrollDelta(pending_scroll_layer));
        }
        break;
      case 1:
        EXPECT_FALSE(impl->pending_tree());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(
                initial_scroll_, main_thread_scroll_ + impl_thread_scroll1_),
            ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(impl_thread_scroll2_, ScrollDelta(scroll_layer));
        EndTest();
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, num_outer_viewport_scrolls_); }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF main_thread_scroll_;
  gfx::Vector2dF impl_thread_scroll1_;
  gfx::Vector2dF impl_thread_scroll2_;
};

// This tests scrolling on the impl side which is only possible with a thread.
MULTI_THREAD_TEST_F(LayerTreeHostScrollTestSimple);

// This test makes sure that layers pick up scrolls that occur between
// beginning a commit and finishing a commit (aka scroll deltas not
// included in sent scroll delta) still apply to layers that don't
// push properties.
class LayerTreeHostScrollTestImplOnlyScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestImplOnlyScroll()
      : initial_scroll_(20, 10), impl_thread_scroll_(-2, 3), impl_scale_(2.f) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void WillCommit() override {
    Layer* scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 0:
        EXPECT_TRUE(base::Contains(
            scroll_layer->layer_tree_host()->LayersThatShouldPushProperties(),
            scroll_layer));
        break;
      case 1:
        // Even if this layer doesn't need push properties, it should
        // still pick up scrolls that happen on the active layer during
        // commit.
        EXPECT_FALSE(base::Contains(
            scroll_layer->layer_tree_host()->LayersThatShouldPushProperties(),
            scroll_layer));
        break;
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    // Scroll after the 2nd commit has started.
    if (impl->active_tree()->source_frame_number() == 0) {
      LayerImpl* active_root = impl->active_tree()->root_layer();
      LayerImpl* active_scroll_layer =
          impl->active_tree()->OuterViewportScrollLayerForTesting();
      ASSERT_TRUE(active_root);
      ASSERT_TRUE(active_scroll_layer);
      active_scroll_layer->ScrollBy(impl_thread_scroll_);
      impl->active_tree()->SetPageScaleOnActiveTree(impl_scale_);
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    // We force a second draw here of the first commit before activating
    // the second commit.
    LayerImpl* active_root = impl->active_tree()->root_layer();
    LayerImpl* active_scroll_layer =
        active_root ? impl->active_tree()->OuterViewportScrollLayerForTesting()
                    : nullptr;
    LayerImpl* pending_root = impl->pending_tree()->root_layer();
    LayerImpl* pending_scroll_layer =
        impl->pending_tree()->OuterViewportScrollLayerForTesting();

    ASSERT_TRUE(pending_root);
    ASSERT_TRUE(pending_scroll_layer);
    switch (impl->pending_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_,
                         ScrollOffsetBase(pending_scroll_layer));
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(pending_scroll_layer));
        EXPECT_FALSE(active_root);
        break;
      case 1:
        // Even though the scroll happened during the commit, both layers
        // should have the appropriate scroll delta.
        EXPECT_VECTOR_EQ(initial_scroll_,
                         ScrollOffsetBase(pending_scroll_layer));
        EXPECT_VECTOR_EQ(impl_thread_scroll_,
                         ScrollDelta(pending_scroll_layer));
        ASSERT_TRUE(active_root);
        EXPECT_VECTOR_EQ(initial_scroll_,
                         ScrollOffsetBase(active_scroll_layer));
        EXPECT_VECTOR_EQ(impl_thread_scroll_, ScrollDelta(active_scroll_layer));
        break;
      case 2:
        // On the next commit, this delta should have been sent and applied.
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, impl_thread_scroll_),
            ScrollOffsetBase(pending_scroll_layer));
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(pending_scroll_layer));
        break;
    }

    // Ensure that the scroll-offsets on the TransformTree are consistent with
    // the synced scroll offsets, for the pending tree.
    if (!impl->pending_tree())
      return;

    LayerImpl* scroll_layer =
        impl->pending_tree()->OuterViewportScrollLayerForTesting();
    gfx::ScrollOffset scroll_offset = CurrentScrollOffset(scroll_layer);
    int transform_index = scroll_layer->transform_tree_index();
    gfx::ScrollOffset transform_tree_scroll_offset =
        impl->pending_tree()
            ->property_trees()
            ->transform_tree.Node(transform_index)
            ->scroll_offset;
    EXPECT_EQ(scroll_offset, transform_tree_scroll_offset);
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (impl->pending_tree())
      impl->SetNeedsRedraw();

    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
        EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
        EXPECT_EQ(1.f, impl->active_tree()->current_page_scale_factor());
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(impl_thread_scroll_, ScrollDelta(scroll_layer));
        EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
        EXPECT_EQ(impl_scale_,
                  impl->active_tree()->current_page_scale_factor());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
        EXPECT_EQ(impl_scale_,
                  impl->active_tree()->current_page_scale_factor());
        EndTest();
        break;
    }
  }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_thread_scroll_;
  float impl_scale_;
};

// This tests scrolling on the impl side which is only possible with a thread.
MULTI_THREAD_TEST_F(LayerTreeHostScrollTestImplOnlyScroll);

// TODO(crbug.com/574283): Mac currently doesn't support smooth scrolling wheel
// events.
#if !defined(OS_MAC)
// This test simulates scrolling on the impl thread such that it starts a scroll
// animation. It ensures that RequestScrollAnimationEndNotification() correctly
// notifies the callback after the animation ends.
class SmoothScrollAnimationEndNotification : public LayerTreeHostScrollTest {
 public:
  SmoothScrollAnimationEndNotification() = default;

  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostScrollTest::InitializeSettings(settings);
    settings->enable_smooth_scroll = true;
  }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    Layer* root_layer = layer_tree_host()->root_layer();
    Layer* root_scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();

    child_layer_ = Layer::Create();
    child_layer_->SetElementId(
        LayerIdToElementIdForTesting(child_layer_->id()));
    child_layer_->SetBounds(gfx::Size(110, 110));

    child_layer_->SetIsDrawable(true);
    child_layer_->SetHitTestable(true);
    child_layer_->SetElementId(
        LayerIdToElementIdForTesting(child_layer_->id()));
    child_layer_->SetBounds(root_scroll_layer->bounds());
    root_layer->AddChild(child_layer_);

    CopyProperties(root_scroll_layer, child_layer_.get());
    CreateTransformNode(child_layer_.get());
    CreateScrollNode(child_layer_.get(), root_layer->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void WillCommit() override {
    // Keep the test committing (otherwise the early out for no update
    // will stall the test).
    if (layer_tree_host()->SourceFrameNumber() < 2) {
      layer_tree_host()->SetNeedsCommit();
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() < 0)
      return;

    if (host_impl->active_tree()->source_frame_number() == 0) {
      const gfx::Point scroll_point(10, 10);
      const gfx::Vector2dF scroll_amount(350, -350);
      auto scroll_state = BeginState(scroll_point, scroll_amount);
      scroll_state->data()->delta_granularity =
          ui::ScrollGranularity::kScrollByPixel;
      InputHandler::ScrollStatus status =
          host_impl->GetInputHandler().ScrollBegin(scroll_state.get(),
                                                   ui::ScrollInputType::kWheel);
      EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
      scroll_state = UpdateState(scroll_point, scroll_amount);
      scroll_state->data()->delta_granularity =
          ui::ScrollGranularity::kScrollByPixel;
      host_impl->GetInputHandler().ScrollUpdate(scroll_state.get());

      EXPECT_TRUE(
          !!host_impl->mutator_host()->ImplOnlyScrollAnimatingElement());
    } else if (!scroll_end_requested_) {
      host_impl->GetInputHandler().ScrollEnd(false);
      scroll_end_requested_ = true;
    }
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    if (scroll_animation_started_)
      return;

    if (layer_tree_host()->HasCompositorDrivenScrollAnimationForTesting()) {
      scroll_animation_started_ = true;
      layer_tree_host()->RequestScrollAnimationEndNotification(
          base::BindOnce(&SmoothScrollAnimationEndNotification::OnScrollEnd,
                         base::Unretained(this)));
    }
  }

  void AfterTest() override {
    EXPECT_TRUE(scroll_end_requested_);
    EXPECT_TRUE(scroll_animation_started_);
    EXPECT_TRUE(scroll_animation_ended_);
  }

 private:
  void OnScrollEnd() {
    scroll_animation_ended_ = true;
    EndTest();
  }

  scoped_refptr<Layer> child_layer_;

  bool scroll_end_requested_ = false;
  bool scroll_animation_started_ = false;
  bool scroll_animation_ended_ = false;
};

MULTI_THREAD_TEST_F(SmoothScrollAnimationEndNotification);
#endif  // !defined(OS_MAC)

void DoGestureScroll(LayerTreeHostImpl* host_impl,
                     const scoped_refptr<Layer>& scroller,
                     gfx::Vector2dF offset) {
  ScrollStateData begin_scroll_state_data;
  begin_scroll_state_data.set_current_native_scrolling_element(
      scroller->element_id());
  begin_scroll_state_data.delta_x_hint = offset.x();
  begin_scroll_state_data.delta_y_hint = offset.y();
  std::unique_ptr<ScrollState> begin_scroll_state(
      new ScrollState(begin_scroll_state_data));
  auto scroll_status = host_impl->GetInputHandler().ScrollBegin(
      begin_scroll_state.get(), ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, scroll_status.thread);
  auto* scrolling_node = host_impl->CurrentlyScrollingNode();
  EXPECT_TRUE(scrolling_node);
  EXPECT_EQ(scrolling_node->element_id, scroller->element_id());

  ScrollStateData update_scroll_state_data;
  update_scroll_state_data.delta_x = offset.x();
  update_scroll_state_data.delta_y = offset.y();
  std::unique_ptr<ScrollState> update_scroll_state(
      new ScrollState(update_scroll_state_data));
  host_impl->GetInputHandler().ScrollUpdate(update_scroll_state.get());

  host_impl->GetInputHandler().ScrollEnd(true /* should_snap */);
}

// This test simulates scrolling on the impl thread such that snapping occurs
// and ensures that the target snap area element ids are sent back to the main
// thread.
class LayerTreeHostScrollTestImplOnlyScrollSnap
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestImplOnlyScrollSnap()
      : initial_scroll_(100, 100),
        impl_thread_scroll_(350, 350),
        snap_area_id_(ElementId(10)) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();
    container_ = Layer::Create();
    scroller_ = Layer::Create();
    scroller_->SetElementId(LayerIdToElementIdForTesting(scroller_->id()));

    container_->SetBounds(gfx::Size(100, 100));
    CopyProperties(root, container_.get());
    root->AddChild(container_);

    scroller_->SetBounds(gfx::Size(1000, 1000));
    CopyProperties(container_.get(), scroller_.get());
    CreateTransformNode(scroller_.get());

    // Set up a snap area.
    snap_area_ = Layer::Create();
    snap_area_->SetBounds(gfx::Size(50, 50));
    snap_area_->SetPosition(gfx::PointF(500, 500));
    CopyProperties(scroller_.get(), snap_area_.get());
    scroller_->AddChild(snap_area_);
    SnapAreaData snap_area_data(ScrollSnapAlign(SnapAlignment::kStart),
                                gfx::RectF(500, 500, 100, 100), false,
                                snap_area_id_);

    // Set up snap container data.
    SnapContainerData snap_container_data(
        ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
        gfx::RectF(0, 0, 100, 100), gfx::ScrollOffset(900, 900));
    snap_container_data.AddSnapAreaData(snap_area_data);
    CreateScrollNode(scroller_.get(), container_->bounds())
        .snap_container_data = snap_container_data;

    root->AddChild(scroller_);
  }

  void BeginTest() override {
    SetScrollOffset(scroller_.get(), initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  // The animations states are updated before this call.
  void WillSendBeginMainFrameOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() < 0)
      return;

    // Perform a scroll such that a snap target is found. This will get pushed
    // to the main thread on the next BeginMainFrame.
    if (host_impl->active_tree()->source_frame_number() == 0) {
      LayerImpl* scroller_impl =
          host_impl->active_tree()->LayerById(scroller_->id());

      DoGestureScroll(host_impl, scroller_, impl_thread_scroll_);

      EXPECT_TRUE(
          host_impl->GetInputHandler().animating_for_snap_for_testing());
      EXPECT_VECTOR_EQ(impl_thread_scroll_, ScrollDelta(scroller_impl));
    } else {
      snap_animation_finished_ =
          !host_impl->GetInputHandler().animating_for_snap_for_testing();
    }
  }

  void UpdateLayerTreeHost() override {
    ScrollNode* scroller_node =
        layer_tree_host()->property_trees()->scroll_tree.Node(
            scroller_->scroll_tree_index());
    auto snap_target_ids = scroller_node->snap_container_data.value()
                               .GetTargetSnapAreaElementIds();
    if (layer_tree_host()->SourceFrameNumber() == 0) {
      // On the first BeginMainFrame scrolling has not happened yet.
      // Check that the scroll offset and scroll snap targets are at the initial
      // values on the main thread.
      EXPECT_VECTOR_EQ(initial_scroll_, CurrentScrollOffset(scroller_.get()));
    }
    if (snap_animation_finished_) {
      // After a snap target is set on the impl thread, the snap targets should
      // be pushed to the main thread.
      EXPECT_EQ(snap_target_ids,
                TargetSnapAreaElementIds(snap_area_id_, snap_area_id_));
      EndTest();
    } else {
      EXPECT_EQ(snap_target_ids, TargetSnapAreaElementIds());
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    PostSetNeedsCommitToMainThread();
  }

 private:
  scoped_refptr<Layer> container_;
  scoped_refptr<Layer> scroller_;
  scoped_refptr<Layer> snap_area_;

  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_thread_scroll_;

  ElementId snap_area_id_;

  bool snap_animation_finished_ = false;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestImplOnlyScrollSnap);

// This test simulates scrolling on the impl thread such that 2 impl-only
// scrolls occur between main frames. It ensures that the snap target ids will
// be synced from impl to main for both snapped scrolling nodes.
class LayerTreeHostScrollTestImplOnlyMultipleScrollSnap
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestImplOnlyMultipleScrollSnap()
      : initial_scroll_(100, 100),
        // Scroll to the boundary so that an animation is not created when
        // snapping to allow 2 scrolls between main frames.
        impl_thread_scroll_a_(400, 400),
        impl_thread_scroll_b_(400, 400),
        snap_area_a_id_(ElementId(10)),
        snap_area_b_id_(ElementId(20)) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();
    container_ = Layer::Create();
    scroller_a_ = Layer::Create();
    scroller_b_ = Layer::Create();
    scroller_a_->SetElementId(LayerIdToElementIdForTesting(scroller_a_->id()));
    scroller_b_->SetElementId(LayerIdToElementIdForTesting(scroller_b_->id()));

    container_->SetBounds(gfx::Size(100, 100));
    CopyProperties(root, container_.get());
    root->AddChild(container_);

    scroller_a_->SetBounds(gfx::Size(1000, 1000));
    CopyProperties(container_.get(), scroller_a_.get());
    CreateTransformNode(scroller_a_.get());
    scroller_b_->SetBounds(gfx::Size(1000, 1000));
    CopyProperties(container_.get(), scroller_b_.get());
    CreateTransformNode(scroller_b_.get());

    // Set up snap areas.
    snap_area_a_ = Layer::Create();
    snap_area_a_->SetBounds(gfx::Size(50, 50));
    snap_area_a_->SetPosition(gfx::PointF(500, 500));
    CopyProperties(scroller_a_.get(), snap_area_a_.get());
    scroller_a_->AddChild(snap_area_a_);
    SnapAreaData snap_area_data_a(ScrollSnapAlign(SnapAlignment::kStart),
                                  gfx::RectF(500, 500, 100, 100), false,
                                  snap_area_a_id_);

    snap_area_b_ = Layer::Create();
    snap_area_b_->SetBounds(gfx::Size(50, 50));
    snap_area_b_->SetPosition(gfx::PointF(500, 500));
    CopyProperties(scroller_b_.get(), snap_area_b_.get());
    scroller_b_->AddChild(snap_area_b_);
    SnapAreaData snap_area_data_b(ScrollSnapAlign(SnapAlignment::kStart),
                                  gfx::RectF(500, 500, 100, 100), false,
                                  snap_area_b_id_);

    // Set up snap container data.
    SnapContainerData snap_container_data_a(
        ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
        gfx::RectF(0, 0, 100, 100), gfx::ScrollOffset(900, 900));
    snap_container_data_a.AddSnapAreaData(snap_area_data_a);
    CreateScrollNode(scroller_a_.get(), container_->bounds())
        .snap_container_data = snap_container_data_a;

    // Set up snap container data.
    SnapContainerData snap_container_data_b(
        ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
        gfx::RectF(0, 0, 100, 100), gfx::ScrollOffset(900, 900));
    snap_container_data_b.AddSnapAreaData(snap_area_data_b);
    CreateScrollNode(scroller_b_.get(), container_->bounds())
        .snap_container_data = snap_container_data_b;

    root->AddChild(scroller_a_);
    root->AddChild(scroller_b_);
  }

  void BeginTest() override {
    SetScrollOffset(scroller_a_.get(), initial_scroll_);
    SetScrollOffset(scroller_b_.get(), initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    ScrollNode* scroller_node_a =
        layer_tree_host()->property_trees()->scroll_tree.Node(
            scroller_a_->scroll_tree_index());
    ScrollNode* scroller_node_b =
        layer_tree_host()->property_trees()->scroll_tree.Node(
            scroller_b_->scroll_tree_index());
    auto snap_target_ids_a = scroller_node_a->snap_container_data.value()
                                 .GetTargetSnapAreaElementIds();
    auto snap_target_ids_b = scroller_node_b->snap_container_data.value()
                                 .GetTargetSnapAreaElementIds();
    if (layer_tree_host()->SourceFrameNumber() == 0) {
      // On the first BeginMainFrame scrolling has not happened yet.
      // Check that the scroll offset and scroll snap targets are at the initial
      // values on the main thread.
      EXPECT_EQ(snap_target_ids_a, TargetSnapAreaElementIds());
      EXPECT_EQ(snap_target_ids_b, TargetSnapAreaElementIds());
      EXPECT_VECTOR_EQ(initial_scroll_, CurrentScrollOffset(scroller_a_.get()));
      EXPECT_VECTOR_EQ(initial_scroll_, CurrentScrollOffset(scroller_b_.get()));
    } else {
      // When scrolling happens on the impl thread, the snap targets of the
      // scrolled layers should be pushed to the main thread.
      EXPECT_EQ(snap_target_ids_a,
                TargetSnapAreaElementIds(snap_area_a_id_, snap_area_a_id_));
      EXPECT_EQ(snap_target_ids_b,
                TargetSnapAreaElementIds(snap_area_b_id_, snap_area_b_id_));
      EndTest();
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    // Perform scrolls such that a snap target is found. These will get pushed
    // to the main thread on the next BeginMainFrame.
    if (host_impl->active_tree()->source_frame_number() == 0) {
      LayerImpl* scroller_impl_a =
          host_impl->active_tree()->LayerById(scroller_a_->id());
      LayerImpl* scroller_impl_b =
          host_impl->active_tree()->LayerById(scroller_b_->id());

      DoGestureScroll(host_impl, scroller_a_, impl_thread_scroll_a_);
      DoGestureScroll(host_impl, scroller_b_, impl_thread_scroll_b_);

      EXPECT_VECTOR_EQ(impl_thread_scroll_a_, ScrollDelta(scroller_impl_a));
      EXPECT_VECTOR_EQ(impl_thread_scroll_b_, ScrollDelta(scroller_impl_b));
    }
    PostSetNeedsCommitToMainThread();
  }

 private:
  scoped_refptr<Layer> container_;
  scoped_refptr<Layer> scroller_a_;
  scoped_refptr<Layer> scroller_b_;
  scoped_refptr<Layer> snap_area_a_;
  scoped_refptr<Layer> snap_area_b_;

  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_thread_scroll_a_;
  gfx::Vector2dF impl_thread_scroll_b_;

  ElementId snap_area_a_id_;
  ElementId snap_area_b_id_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestImplOnlyMultipleScrollSnap);

class LayerTreeHostScrollTestScrollZeroMaxScrollOffset
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollZeroMaxScrollOffset() = default;

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    // Add a sub-scroller to test ScrollBegin against. The outer viewport scroll
    // will be latched to for scrolling even if it doesn't have any scroll
    // extent in the given direction to support overscroll actions.
    scroller_ = Layer::Create();
    scroller_->SetIsDrawable(true);
    scroller_->SetHitTestable(true);
    scroller_->SetBounds(gfx::Size(200, 200));
    scroller_->SetElementId(LayerIdToElementIdForTesting(scroller_->id()));
    CopyProperties(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                   scroller_.get());
    CreateTransformNode(scroller_.get());
    CreateScrollNode(scroller_.get(),
                     layer_tree_host()->root_layer()->bounds());
    layer_tree_host()->root_layer()->AddChild(scroller_.get());
  }

  void BeginTest() override { NextStep(); }

  void NextStep() {
    if (TestEnded())
      return;

    ++cur_step_;

    ScrollTree& scroll_tree = layer_tree_host()->property_trees()->scroll_tree;
    ScrollNode* scroll_node = scroll_tree.Node(scroller_->scroll_tree_index());
    switch (cur_step_) {
      case 1:
        // Set max_scroll_offset = (100, 100).
        scroll_node->bounds = scroll_node->container_bounds;
        scroll_node->bounds.Enlarge(100, 100);
        break;
      case 2:
        // Set max_scroll_offset = (0, 0).
        scroll_node->bounds = scroll_node->container_bounds;
        break;
      case 3:
        // Set max_scroll_offset = (-1, -1).
        scroll_node->bounds = gfx::Size();
        break;
    }

    layer_tree_host()->SetNeedsCommit();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (TestEnded())
      return;

    ScrollTree& scroll_tree =
        impl->active_tree()->property_trees()->scroll_tree;
    ScrollNode* scroll_node = scroll_tree.Node(scroller_->scroll_tree_index());

    ScrollStateData scroll_state_data;
    scroll_state_data.is_beginning = true;
    // The position has to be at (0, 0) since the viewport in this test has
    // bounds (1, 1).
    scroll_state_data.position_x = 0;
    scroll_state_data.position_y = 0;
    scroll_state_data.delta_x_hint = 10;
    scroll_state_data.delta_y_hint = 10;
    scroll_state_data.is_direct_manipulation = true;

    ScrollState scroll_state(scroll_state_data);
    InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
        &scroll_state, ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread)
        << "In Frame " << impl->active_tree()->source_frame_number();

    switch (cur_step_) {
      case 1:
        // Since the scroller has scroll extend and is scrollable, we should
        // have targeted it.
        EXPECT_EQ(scroll_node, impl->CurrentlyScrollingNode()) << "In Frame 0";
        break;
      case 2:
        // Since the max_scroll_offset is (0, 0) - we shouldn't target it and
        // we should instead bubble up to the viewport.
        EXPECT_EQ(impl->OuterViewportScrollNode(),
                  impl->CurrentlyScrollingNode())
            << "In Frame 1";
        break;
      case 3:
        // Since the max_scroll_offset is (-1, -1) - we shouldn't target it and
        // we should instead bubble up to the viewport.
        EXPECT_EQ(impl->OuterViewportScrollNode(),
                  impl->CurrentlyScrollingNode())
            << "In Frame 2";
        EndTest();
        break;
    }
    impl->GetInputHandler().ScrollEnd();
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LayerTreeHostScrollTestScrollZeroMaxScrollOffset::NextStep,
            base::Unretained(this)));
  }

 private:
  int cur_step_ = 0;
  scoped_refptr<Layer> scroller_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestScrollZeroMaxScrollOffset);

class LayerTreeHostScrollTestScrollNonDrawnLayer
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollNonDrawnLayer() = default;

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->OuterViewportScrollLayerForTesting()->SetIsDrawable(
        false);
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    gfx::ScrollOffset(20.f, 20.f));
    layer_tree_host()
        ->OuterViewportScrollLayerForTesting()
        ->SetNonFastScrollableRegion(gfx::Rect(20, 20, 20, 20));
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    // Verify that the scroll layer's scroll offset is taken into account when
    // checking whether the screen space point is inside the non-fast
    // scrollable region.
    InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 1)).get(),
        ui::ScrollInputType::kTouchscreen);
    if (base::FeatureList::IsEnabled(features::kScrollUnification)) {
      EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
      EXPECT_TRUE(status.needs_main_thread_hit_test);
      impl->GetInputHandler().ScrollEnd();
    } else {
      EXPECT_EQ(ScrollThread::SCROLL_ON_MAIN_THREAD, status.thread);
      EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
                status.main_thread_scrolling_reasons);
    }

    status = impl->GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(21, 21), gfx::Vector2dF(0, 1)).get(),
        ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_FALSE(status.needs_main_thread_hit_test);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_scrolling_reasons);

    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollNonDrawnLayer);

class LayerTreeHostScrollTestImplScrollUnderMainThreadScrollingParent
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestImplScrollUnderMainThreadScrollingParent() {
    SetUseLayerLists();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    GetScrollNode(layer_tree_host()->OuterViewportScrollLayerForTesting())
        ->main_thread_scrolling_reasons =
        MainThreadScrollingReason::kThreadedScrollingDisabled;

    scroller_ = Layer::Create();
    scroller_->SetIsDrawable(true);
    scroller_->SetHitTestable(true);
    scroller_->SetBounds(gfx::Size(200, 200));
    scroller_->SetElementId(LayerIdToElementIdForTesting(scroller_->id()));
    CopyProperties(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                   scroller_.get());
    CreateTransformNode(scroller_.get());
    CreateScrollNode(scroller_.get(),
                     layer_tree_host()->root_layer()->bounds());
    layer_tree_host()->root_layer()->AddChild(scroller_.get());
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    ScrollTree& scroll_tree =
        impl->active_tree()->property_trees()->scroll_tree;
    ScrollNode* scroller_scroll_node =
        scroll_tree.Node(scroller_->scroll_tree_index());

    ScrollStateData scroll_state_data;
    scroll_state_data.is_beginning = true;
    // To hit the scroller, the position has to be at (0, 0) since the viewport
    // in this test has bounds (1, 1) and would otherwise clip the hit test.
    scroll_state_data.position_x = 0;
    scroll_state_data.position_y = 0;
    scroll_state_data.delta_x_hint = 10;
    scroll_state_data.delta_y_hint = 10;
    scroll_state_data.is_direct_manipulation = true;
    ScrollState scroll_state(scroll_state_data);

    // Scroll hitting the scroller layer.
    {
      InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
          &scroll_state, ui::ScrollInputType::kTouchscreen);
      if (base::FeatureList::IsEnabled(features::kScrollUnification)) {
        EXPECT_EQ(impl->CurrentlyScrollingNode(), scroller_scroll_node);
        EXPECT_FALSE(status.needs_main_thread_hit_test);
      } else {
        // Despite the fact that we hit the scroller, which has no main thread
        // scrolling reason, we still must fallback to main thread scrolling due
        // to the fact that it has a main thread scrolling ancestor.
        EXPECT_EQ(impl->CurrentlyScrollingNode(), nullptr);
        EXPECT_EQ(ScrollThread::SCROLL_ON_MAIN_THREAD, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kThreadedScrollingDisabled,
                  status.main_thread_scrolling_reasons);
      }
      impl->GetInputHandler().ScrollEnd();
    }

    // Scroll hitting the viewport layer.
    {
      // A hit test outside the viewport should fallback to scrolling the
      // viewport.
      scroll_state.data()->position_y = 1000;

      InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
          &scroll_state, ui::ScrollInputType::kTouchscreen);
      if (base::FeatureList::IsEnabled(features::kScrollUnification)) {
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        EXPECT_FALSE(status.needs_main_thread_hit_test);
        EXPECT_EQ(impl->CurrentlyScrollingNode(),
                  impl->OuterViewportScrollNode());
      } else {
        // Since the viewport has a main thread scrolling reason, this
        // too should fallback to the main thread.
        EXPECT_EQ(impl->CurrentlyScrollingNode(), nullptr);
        EXPECT_EQ(ScrollThread::SCROLL_ON_MAIN_THREAD, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kThreadedScrollingDisabled,
                  status.main_thread_scrolling_reasons);
      }
      impl->GetInputHandler().ScrollEnd();
    }

    EndTest();
  }

 private:
  scoped_refptr<Layer> scroller_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestImplScrollUnderMainThreadScrollingParent);

class ThreadCheckingInputHandlerClient : public InputHandlerClient {
 public:
  ThreadCheckingInputHandlerClient(base::SingleThreadTaskRunner* runner,
                                   bool* received_stop_flinging)
      : task_runner_(runner), received_stop_flinging_(received_stop_flinging) {}

  void WillShutdown() override {
    if (!received_stop_flinging_)
      ADD_FAILURE() << "WillShutdown() called before fling stopped";
  }

  void Animate(base::TimeTicks time) override {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "Animate called on wrong thread";
  }

  void ReconcileElasticOverscrollAndRootScroll() override {
    if (!task_runner_->BelongsToCurrentThread()) {
      ADD_FAILURE() << "ReconcileElasticOverscrollAndRootScroll called on "
                    << "wrong thread";
    }
  }

  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override {
    if (!task_runner_->BelongsToCurrentThread()) {
      ADD_FAILURE() << "UpdateRootLayerStateForSynchronousInputHandler called "
                    << " on wrong thread";
    }
  }

  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override {
    if (!task_runner_->BelongsToCurrentThread()) {
      ADD_FAILURE() << "DeliverInputForBeginFrame called on wrong thread";
    }
  }

  void DeliverInputForHighLatencyMode() override {
    if (!task_runner_->BelongsToCurrentThread()) {
      ADD_FAILURE() << "DeliverInputForHighLatencyMode called on wrong thread";
    }
  }

 private:
  base::SingleThreadTaskRunner* task_runner_;
  bool* received_stop_flinging_;
};

class LayerTreeHostScrollTestLayerStructureChange
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestLayerStructureChange()
      : scroll_destroy_whole_tree_(false) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    Layer* outer_scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();

    Layer* root_scroll_layer = CreateScrollLayer(outer_scroll_layer);
    Layer* sibling_scroll_layer = CreateScrollLayer(outer_scroll_layer);
    Layer* child_scroll_layer = CreateScrollLayer(root_scroll_layer);
    root_scroll_layer_id_ = root_scroll_layer->id();
    sibling_scroll_layer_id_ = sibling_scroll_layer->id();
    child_scroll_layer_id_ = child_scroll_layer->id();
    fake_content_layer_client_.set_bounds(root_layer->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        SetScrollOffsetDelta(
            impl->active_tree()->LayerById(root_scroll_layer_id_),
            gfx::Vector2dF(5, 5));
        SetScrollOffsetDelta(
            impl->active_tree()->LayerById(child_scroll_layer_id_),
            gfx::Vector2dF(5, 5));
        SetScrollOffsetDelta(
            impl->active_tree()->LayerById(sibling_scroll_layer_id_),
            gfx::Vector2dF(5, 5));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EndTest();
        break;
    }
  }

  void DidScroll(ElementId element_id,
                 const gfx::ScrollOffset&,
                 const base::Optional<TargetSnapAreaElementIds>&) override {
    if (scroll_destroy_whole_tree_) {
      layer_tree_host()->SetRootLayer(nullptr);
      layer_tree_host()->property_trees()->clear();
      layer_tree_host()->RegisterViewportPropertyIds(
          LayerTreeHost::ViewportPropertyIds());
      EndTest();
      return;
    }
    layer_tree_host()->LayerByElementId(element_id)->RemoveFromParent();
  }

 protected:
  Layer* CreateScrollLayer(Layer* parent) {
    scoped_refptr<PictureLayer> scroll_layer =
        PictureLayer::Create(&fake_content_layer_client_);
    scroll_layer->SetIsDrawable(true);
    scroll_layer->SetHitTestable(true);
    scroll_layer->SetElementId(
        LayerIdToElementIdForTesting(scroll_layer->id()));
    scroll_layer->SetBounds(gfx::Size(parent->bounds().width() + 100,
                                      parent->bounds().height() + 100));

    CopyProperties(parent, scroll_layer.get());
    CreateTransformNode(scroll_layer.get());
    CreateScrollNode(scroll_layer.get(), parent->bounds());
    layer_tree_host()->root_layer()->AddChild(scroll_layer);

    return scroll_layer.get();
  }

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree.SetScrollOffsetDeltaForTesting(
                layer_impl->element_id(), delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
          layer_impl->element_id());
  }

  int root_scroll_layer_id_;
  int sibling_scroll_layer_id_;
  int child_scroll_layer_id_;

  FakeContentLayerClient fake_content_layer_client_;

  bool scroll_destroy_whole_tree_;
};

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyLayer) {
  RunTest(CompositorMode::THREADED);
}

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyWholeTree) {
  scroll_destroy_whole_tree_ = true;
  RunTest(CompositorMode::THREADED);
}

class LayerTreeHostScrollTestScrollMFBA : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMFBA()
      : initial_scroll_(10, 20),
        second_scroll_(40, 5),
        third_scroll_(20, 10),
        scroll_amount_(2, -1),
        num_commits_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostScrollTest::InitializeSettings(settings);
    settings->main_frame_before_activation_enabled = true;
  }

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void ReadyToCommitOnThread(LayerTreeHostImpl* impl) override {
    switch (num_commits_) {
      case 1:
        // Ask for commit here because activation (and draw) will be blocked.
        impl->SetNeedsCommit();
        // Block activation after second commit until third commit is ready.
        impl->BlockNotifyReadyToActivateForTesting(true);
        break;
      case 2:
        // Unblock activation after third commit is ready.
        impl->BlockNotifyReadyToActivateForTesting(false);
        break;
    }
    num_commits_++;
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, CurrentScrollOffset(scroll_layer));
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, scroll_amount_),
            CurrentScrollOffset(scroll_layer));
        // Pretend like Javascript updated the scroll position itself.
        SetScrollOffset(scroll_layer, second_scroll_);
        break;
      case 2:
        // Third frame does not see a scroll delta because we only did one
        // scroll for the second and third frames.
        EXPECT_VECTOR_EQ(second_scroll_, CurrentScrollOffset(scroll_layer));
        // Pretend like Javascript updated the scroll position itself.
        SetScrollOffset(scroll_layer, third_scroll_);
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
        Scroll(impl);
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        // Ask for commit after we've scrolled.
        impl->SetNeedsCommit();
        break;
      case 1:
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
        EXPECT_VECTOR_EQ(second_scroll_, ScrollOffsetBase(scroll_layer));
        Scroll(impl);
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        break;
      case 2:
        // The scroll hasn't been consumed by the main thread.
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        EXPECT_VECTOR_EQ(third_scroll_, ScrollOffsetBase(scroll_layer));
        EndTest();
        break;
    }
  }

  void AfterTest() override {
    EXPECT_EQ(3, num_commits_);
    EXPECT_EQ(1, num_outer_viewport_scrolls_);
  }

 private:
  void Scroll(LayerTreeHostImpl* impl) {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();

    scroll_layer->SetBounds(
        gfx::Size(root->bounds().width() + 100, root->bounds().height() + 100));
    scroll_layer->ScrollBy(scroll_amount_);
  }

  gfx::ScrollOffset initial_scroll_;
  gfx::ScrollOffset second_scroll_;
  gfx::ScrollOffset third_scroll_;
  gfx::Vector2dF scroll_amount_;
  int num_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMFBA);

class LayerTreeHostScrollTestScrollAbortedCommitMFBA
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollAbortedCommitMFBA()
      : initial_scroll_(50, 60),
        impl_scroll_(-3, 2),
        second_main_scroll_(14, -3),
        num_will_begin_main_frames_(0),
        num_did_begin_main_frames_(0),
        num_will_commits_(0),
        num_did_commits_(0),
        num_impl_commits_(0),
        num_aborted_commits_(0),
        num_draws_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostScrollTest::InitializeSettings(settings);
    settings->main_frame_before_activation_enabled = true;
  }

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    gfx::Size scroll_layer_bounds(200, 200);
    layer_tree_host()->OuterViewportScrollLayerForTesting()->SetBounds(
        scroll_layer_bounds);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void WillBeginMainFrame() override {
    num_will_begin_main_frames_++;
    Layer* root_scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    switch (num_will_begin_main_frames_) {
      case 1:
        // This will not be aborted because of the initial prop changes.
        EXPECT_EQ(0, num_outer_viewport_scrolls_);
        EXPECT_EQ(0, layer_tree_host()->SourceFrameNumber());
        EXPECT_VECTOR_EQ(initial_scroll_,
                         CurrentScrollOffset(root_scroll_layer));
        break;
      case 2:
        // This commit will not be aborted because of the scroll change.
        EXPECT_EQ(1, num_outer_viewport_scrolls_);
        EXPECT_EQ(1, layer_tree_host()->SourceFrameNumber());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, impl_scroll_),
            CurrentScrollOffset(root_scroll_layer));
        SetScrollOffset(
            root_scroll_layer,
            gfx::ScrollOffsetWithDelta(CurrentScrollOffset(root_scroll_layer),
                                       second_main_scroll_));
        break;
      case 3: {
        // This commit will be aborted.
        EXPECT_EQ(2, num_outer_viewport_scrolls_);
        // The source frame number still increases even with the abort.
        EXPECT_EQ(2, layer_tree_host()->SourceFrameNumber());
        gfx::Vector2dF delta =
            impl_scroll_ + impl_scroll_ + second_main_scroll_;
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                         CurrentScrollOffset(root_scroll_layer));
        break;
      }
      case 4: {
        // This commit will also be aborted.
        EXPECT_EQ(3, num_outer_viewport_scrolls_);
        EXPECT_EQ(3, layer_tree_host()->SourceFrameNumber());
        gfx::Vector2dF delta =
            impl_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_;
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                         CurrentScrollOffset(root_scroll_layer));
        break;
      }
    }
  }

  void DidBeginMainFrame() override { num_did_begin_main_frames_++; }

  void WillCommit() override { num_will_commits_++; }

  void DidCommit() override { num_did_commits_++; }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    switch (num_impl_commits_) {
      case 1:
        // Redraw so that we keep scrolling.
        impl->SetNeedsRedraw();
        // Block activation until third commit is aborted.
        impl->BlockNotifyReadyToActivateForTesting(true);
        break;
    }
    num_impl_commits_++;
  }

  void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* impl,
                                     CommitEarlyOutReason reason) override {
    switch (num_aborted_commits_) {
      case 0:
        EXPECT_EQ(2, num_impl_commits_);
        // Unblock activation when third commit is aborted.
        impl->BlockNotifyReadyToActivateForTesting(false);
        break;
      case 1:
        EXPECT_EQ(2, num_impl_commits_);
        // Redraw to end the test.
        impl->SetNeedsRedraw();
        break;
    }
    num_aborted_commits_++;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root_scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();
    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        switch (num_impl_commits_) {
          case 1: {
            // First draw
            EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
            root_scroll_layer->ScrollBy(impl_scroll_);
            EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
            EXPECT_VECTOR_EQ(initial_scroll_,
                             ScrollOffsetBase(root_scroll_layer));
            impl->SetNeedsCommit();
            break;
          }
          case 2: {
            // Second draw but no new active tree because activation is blocked.
            EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
            root_scroll_layer->ScrollBy(impl_scroll_);
            EXPECT_VECTOR_EQ(impl_scroll_ + impl_scroll_,
                             ScrollDelta(root_scroll_layer));
            EXPECT_VECTOR_EQ(initial_scroll_,
                             ScrollOffsetBase(root_scroll_layer));
            // Ask for another commit (which will abort).
            impl->SetNeedsCommit();
            break;
          }
          default:
            NOTREACHED();
        }
        break;
      }
      case 1: {
        EXPECT_EQ(2, num_impl_commits_);
        // All scroll deltas so far should be consumed.
        EXPECT_EQ(gfx::ScrollOffset(), ScrollDelta(root_scroll_layer));
        switch (num_aborted_commits_) {
          case 1: {
            root_scroll_layer->ScrollBy(impl_scroll_);
            EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
            gfx::Vector2dF prev_delta =
                impl_scroll_ + impl_scroll_ + second_main_scroll_;
            EXPECT_VECTOR_EQ(
                gfx::ScrollOffsetWithDelta(initial_scroll_, prev_delta),
                ScrollOffsetBase(root_scroll_layer));
            // Ask for another commit (which will abort).
            impl->SetNeedsCommit();
            break;
          }
          case 2: {
            gfx::Vector2dF delta = impl_scroll_ + impl_scroll_ + impl_scroll_ +
                                   second_main_scroll_;
            EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                             ScrollOffsetBase(root_scroll_layer));
            // End test after second aborted commit (fourth commit request).
            EndTest();
            break;
          }
        }
        break;
      }
    }
    num_draws_++;
  }

  void AfterTest() override {
    EXPECT_EQ(3, num_outer_viewport_scrolls_);
    // Verify that the embedder sees aborted commits as real commits.
    EXPECT_EQ(4, num_will_begin_main_frames_);
    EXPECT_EQ(4, num_did_begin_main_frames_);
    EXPECT_EQ(4, num_will_commits_);
    EXPECT_EQ(4, num_did_commits_);
    // ...but the compositor thread only sees two real ones.
    EXPECT_EQ(2, num_impl_commits_);
    // ...and two aborted ones.
    EXPECT_EQ(2, num_aborted_commits_);
    // ...and four draws.
    EXPECT_EQ(4, num_draws_);
  }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_scroll_;
  gfx::Vector2dF second_main_scroll_;
  int num_will_begin_main_frames_;
  int num_did_begin_main_frames_;
  int num_will_commits_;
  int num_did_commits_;
  int num_impl_commits_;
  int num_aborted_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollAbortedCommitMFBA);

class MockInputHandlerClient : public InputHandlerClient {
 public:
  MockInputHandlerClient() = default;

  MOCK_METHOD0(ReconcileElasticOverscrollAndRootScroll, void());

  void WillShutdown() override {}
  void Animate(base::TimeTicks) override {}
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override {}
  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override {}
  void DeliverInputForHighLatencyMode() override {}
};

// This is a regression test, see crbug.com/639046.
class LayerTreeHostScrollTestElasticOverscroll
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestElasticOverscroll()
      : num_begin_main_frames_impl_thread_(0),
        scroll_elasticity_helper_(nullptr),
        num_begin_main_frames_main_thread_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostScrollTest::InitializeSettings(settings);
    settings->enable_elastic_overscroll = true;
  }

  void BeginTest() override {
    DCHECK(HasImplThread());
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LayerTreeHostScrollTestElasticOverscroll::BindInputHandler,
            base::Unretained(this), layer_tree_host()->GetDelegateForInput()));
    PostSetNeedsCommitToMainThread();
  }

  void BindInputHandler(base::WeakPtr<CompositorDelegateForInput> delegate) {
    DCHECK(task_runner_provider()->IsImplThread());
    base::WeakPtr<InputHandler> input_handler = InputHandler::Create(*delegate);
    input_handler->BindToClient(&input_handler_client_);
    scroll_elasticity_helper_ = input_handler->CreateScrollElasticityHelper();
    DCHECK(scroll_elasticity_helper_);
  }

  void ApplyViewportChanges(const ApplyViewportChangesArgs& args) override {
    DCHECK_NE(0, num_begin_main_frames_main_thread_)
        << "The first BeginMainFrame has no deltas to report";
    DCHECK_LT(num_begin_main_frames_main_thread_, 5);

    gfx::Vector2dF expected_elastic_overscroll =
        elastic_overscroll_test_cases_[num_begin_main_frames_main_thread_];
    current_elastic_overscroll_ += args.elastic_overscroll_delta;
    EXPECT_EQ(expected_elastic_overscroll, current_elastic_overscroll_);
    EXPECT_EQ(expected_elastic_overscroll,
              layer_tree_host()->elastic_overscroll());
  }

  void WillBeginMainFrame() override { num_begin_main_frames_main_thread_++; }

  void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* host_impl,
                                     CommitEarlyOutReason reason) override {
    VerifyBeginMainFrameResultOnImplThread(host_impl, true);
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    VerifyBeginMainFrameResultOnImplThread(host_impl, false);
  }

  void VerifyBeginMainFrameResultOnImplThread(LayerTreeHostImpl* host_impl,
                                              bool begin_main_frame_aborted) {
    gfx::Vector2dF expected_elastic_overscroll =
        elastic_overscroll_test_cases_[num_begin_main_frames_impl_thread_];
    EXPECT_EQ(expected_elastic_overscroll,
              scroll_elasticity_helper_->StretchAmount());
    if (!begin_main_frame_aborted)
      EXPECT_EQ(
          expected_elastic_overscroll,
          host_impl->pending_tree()->elastic_overscroll()->Current(false));

    ++num_begin_main_frames_impl_thread_;
    gfx::Vector2dF next_test_case;
    if (num_begin_main_frames_impl_thread_ < 5)
      next_test_case =
          elastic_overscroll_test_cases_[num_begin_main_frames_impl_thread_];

    switch (num_begin_main_frames_impl_thread_) {
      case 1:
        // The first BeginMainFrame is never aborted.
        EXPECT_FALSE(begin_main_frame_aborted);
        scroll_elasticity_helper_->SetStretchAmount(next_test_case);
        break;
      case 2:
        EXPECT_TRUE(begin_main_frame_aborted);
        scroll_elasticity_helper_->SetStretchAmount(next_test_case);

        // Since the elastic overscroll is never mutated on the main thread, the
        // BeginMainFrame which reports the delta is aborted. Post a commit
        // request to the main thread to make sure it goes through.
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        EXPECT_FALSE(begin_main_frame_aborted);
        scroll_elasticity_helper_->SetStretchAmount(next_test_case);
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        EXPECT_FALSE(begin_main_frame_aborted);
        scroll_elasticity_helper_->SetStretchAmount(next_test_case);
        break;
      case 5:
        EXPECT_TRUE(begin_main_frame_aborted);
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (num_begin_main_frames_impl_thread_ == 5)
      return;

    // Ensure that the elastic overscroll value on the active tree remains
    // unmodified after activation.
    gfx::Vector2dF expected_elastic_overscroll =
        elastic_overscroll_test_cases_[num_begin_main_frames_impl_thread_];
    EXPECT_EQ(expected_elastic_overscroll,
              scroll_elasticity_helper_->StretchAmount());
  }

  void WillPrepareToDrawOnThread(LayerTreeHostImpl* host_impl) override {
    // The InputHandlerClient must receive a call to reconcile the overscroll
    // before each draw.
    EXPECT_CALL(input_handler_client_,
                ReconcileElasticOverscrollAndRootScroll())
        .Times(1);
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    Mock::VerifyAndClearExpectations(&input_handler_client_);
    return draw_result;
  }

  void AfterTest() override {
    EXPECT_EQ(num_begin_main_frames_impl_thread_, 5);
    EXPECT_EQ(num_begin_main_frames_main_thread_, 5);
    gfx::Vector2dF expected_elastic_overscroll =
        elastic_overscroll_test_cases_[4];
    EXPECT_EQ(expected_elastic_overscroll, current_elastic_overscroll_);
  }

 private:
  // These values should be used on the impl thread only.
  int num_begin_main_frames_impl_thread_;
  MockInputHandlerClient input_handler_client_;
  ScrollElasticityHelper* scroll_elasticity_helper_;

  // These values should be used on the main thread only.
  int num_begin_main_frames_main_thread_;
  gfx::Vector2dF current_elastic_overscroll_;

  const gfx::Vector2dF elastic_overscroll_test_cases_[5] = {
      gfx::Vector2dF(0, 0), gfx::Vector2dF(5, 10), gfx::Vector2dF(5, 5),
      gfx::Vector2dF(-4, -5), gfx::Vector2dF(0, 0)};
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestElasticOverscroll);

class LayerTreeHostScrollTestPropertyTreeUpdate
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestPropertyTreeUpdate()
      : initial_scroll_(10, 20), second_scroll_(0, 0) {}

  void BeginTest() override {
    SetScrollOffset(layer_tree_host()->OuterViewportScrollLayerForTesting(),
                    initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    if (layer_tree_host()->SourceFrameNumber() == 0) {
      EXPECT_VECTOR_EQ(initial_scroll_, CurrentScrollOffset(scroll_layer));
    } else {
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_, scroll_amount_),
          CurrentScrollOffset(scroll_layer));
      SetScrollOffset(scroll_layer, second_scroll_);
      SetOpacity(scroll_layer, 0.5f);
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer =
        impl->active_tree()->OuterViewportScrollLayerForTesting();

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(initial_scroll_,
                         GetTransformNode(scroll_layer)->scroll_offset);
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(second_scroll_, ScrollOffsetBase(scroll_layer));
        EXPECT_VECTOR_EQ(second_scroll_,
                         GetTransformNode(scroll_layer)->scroll_offset);
        EndTest();
        break;
    }
  }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::ScrollOffset second_scroll_;
  gfx::Vector2dF scroll_amount_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostScrollTestPropertyTreeUpdate);

class LayerTreeHostScrollTestImplSideInvalidation
    : public LayerTreeHostScrollTest {
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidScrollOuterViewport(const gfx::ScrollOffset& offset) override {
    LayerTreeHostScrollTest::DidScrollOuterViewport(offset);

    // Defer responding to the main frame until an impl-side pending tree is
    // created for the invalidation request.
    {
      CompletionEvent completion;
      task_runner_provider()->ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&LayerTreeHostScrollTestImplSideInvalidation::
                             WaitForInvalidationOnImplThread,
                         base::Unretained(this), &completion));
      completion.Wait();
    }

    switch (++num_of_deltas_) {
      case 1: {
        // First set of deltas is here. The impl thread will scroll to the
        // second case on activation, so add a delta from the main thread that
        // takes us to the final value.
        Layer* outer_viewport_layer =
            layer_tree_host()->OuterViewportScrollLayerForTesting();
        gfx::ScrollOffset delta_to_send =
            outer_viewport_offsets_[2] - outer_viewport_offsets_[1];
        SetScrollOffset(
            outer_viewport_layer,
            CurrentScrollOffset(outer_viewport_layer) + delta_to_send);
      } break;
      case 2:
        // Let the commit abort for the second set of deltas.
        break;
      default:
        NOTREACHED();
    }
  }

  void WaitForInvalidationOnImplThread(CompletionEvent* completion) {
    impl_side_invalidation_event_ = completion;
    SignalCompletionIfPossible();
  }

  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    invalidated_on_impl_thread_ = true;
    SignalCompletionIfPossible();
  }

  void WillNotifyReadyToActivateOnThread(
      LayerTreeHostImpl* host_impl) override {
    // Ensure that the scroll-offsets on the TransformTree are consistent with
    // the synced scroll offsets, for the pending tree.
    if (!host_impl->pending_tree())
      return;

    LayerImpl* scroll_layer =
        host_impl->pending_tree()->OuterViewportScrollLayerForTesting();
    gfx::ScrollOffset scroll_offset = CurrentScrollOffset(scroll_layer);
    int transform_index = scroll_layer->transform_tree_index();
    gfx::ScrollOffset transform_tree_scroll_offset =
        host_impl->pending_tree()
            ->property_trees()
            ->transform_tree.Node(transform_index)
            ->scroll_offset;
    EXPECT_EQ(scroll_offset, transform_tree_scroll_offset);
  }

  void SignalCompletionIfPossible() {
    if (!invalidated_on_impl_thread_ || !impl_side_invalidation_event_)
      return;

    impl_side_invalidation_event_->Signal();
    impl_side_invalidation_event_ = nullptr;
    invalidated_on_impl_thread_ = false;
  }

  void DidSendBeginMainFrameOnThread(LayerTreeHostImpl* host_impl) override {
    switch (++num_of_main_frames_) {
      case 1:
        // Do nothing for the first BeginMainFrame.
        break;
      case 2:
        // Add some more delta to the active tree state of the scroll offset and
        // a commit to send this additional delta to the main thread.
        host_impl->active_tree()
            ->OuterViewportScrollLayerForTesting()
            ->SetCurrentScrollOffset(outer_viewport_offsets_[1]);
        host_impl->SetNeedsCommit();

        // Request an impl-side invalidation to create an impl-side pending
        // tree.
        host_impl->RequestImplSideInvalidationForCheckerImagedTiles();
        break;
      case 3:
        // Request another impl-side invalidation so the aborted commit comes
        // after this tree is activated.
        host_impl->RequestImplSideInvalidationForCheckerImagedTiles();
        break;
      default:
        NOTREACHED();
    }
  }

  void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* host_impl,
                                     CommitEarlyOutReason reason) override {
    EXPECT_EQ(CommitEarlyOutReason::FINISHED_NO_UPDATES, reason);
    EXPECT_EQ(3, num_of_main_frames_);
    EXPECT_EQ(
        outer_viewport_offsets_[2],
        CurrentScrollOffset(
            host_impl->active_tree()->OuterViewportScrollLayerForTesting()));
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    switch (++num_of_activations_) {
      case 1:
        // Now that we have the active tree, scroll a layer and ask for a commit
        // to send a BeginMainFrame with the scroll delta to the main thread.
        host_impl->active_tree()
            ->OuterViewportScrollLayerForTesting()
            ->SetCurrentScrollOffset(outer_viewport_offsets_[0]);
        host_impl->SetNeedsCommit();
        break;
      case 2:
        // The second activation is from an impl-side pending tree so the source
        // frame number on the active tree remains unchanged, and the scroll
        // offset on the active tree should also remain unchanged.
        EXPECT_EQ(0, host_impl->active_tree()->source_frame_number());
        EXPECT_EQ(
            outer_viewport_offsets_[1],
            CurrentScrollOffset(host_impl->active_tree()
                                    ->OuterViewportScrollLayerForTesting()));
        break;
      case 3:
        // The third activation is from a commit. The scroll offset on the
        // active tree should include deltas sent from the main thread.
        EXPECT_EQ(1, host_impl->active_tree()->source_frame_number());
        EXPECT_EQ(
            outer_viewport_offsets_[2],
            CurrentScrollOffset(host_impl->active_tree()
                                    ->OuterViewportScrollLayerForTesting()));
        break;
      case 4:
        // The fourth activation is from an impl-side pending tree, which should
        // leave the scroll offset unchanged.
        EXPECT_EQ(1, host_impl->active_tree()->source_frame_number());
        EXPECT_EQ(
            outer_viewport_offsets_[2],
            CurrentScrollOffset(host_impl->active_tree()
                                    ->OuterViewportScrollLayerForTesting()));
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void AfterTest() override {
    EXPECT_EQ(4, num_of_activations_);
    EXPECT_EQ(2, num_of_deltas_);
    EXPECT_EQ(3, num_of_main_frames_);
  }

  const gfx::ScrollOffset outer_viewport_offsets_[3] = {
      gfx::ScrollOffset(20, 20), gfx::ScrollOffset(50, 50),
      gfx::ScrollOffset(70, 70)};

  // Impl thread.
  int num_of_activations_ = 0;
  int num_of_main_frames_ = 0;
  bool invalidated_on_impl_thread_ = false;
  CompletionEvent* impl_side_invalidation_event_ = nullptr;

  // Main thread.
  int num_of_deltas_ = 0;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestImplSideInvalidation);

class NonScrollingNonFastScrollableRegion : public LayerTreeHostScrollTest {
 public:
  NonScrollingNonFastScrollableRegion() { SetUseLayerLists(); }

  // Setup 3 Layers:
  // 1) bottom_ which has a non-fast region in the bottom-right.
  // 2) middle_scrollable_ which is scrollable.
  // 3) top_ which has a non-fast region in the top-left and is offset by
  //    |middle_scrollable_|'s scroll offset.
  void SetupTree() override {
    SetInitialRootBounds(gfx::Size(800, 600));
    LayerTreeHostScrollTest::SetupTree();
    Layer* outer_scroll =
        layer_tree_host()->OuterViewportScrollLayerForTesting();
    fake_content_layer_client_.set_bounds(outer_scroll->bounds());

    bottom_ = FakePictureLayer::Create(&fake_content_layer_client_);
    bottom_->SetElementId(LayerIdToElementIdForTesting(bottom_->id()));
    bottom_->SetBounds(gfx::Size(100, 100));
    bottom_->SetNonFastScrollableRegion(Region(gfx::Rect(50, 50, 50, 50)));
    bottom_->SetHitTestable(true);
    CopyProperties(outer_scroll, bottom_.get());
    outer_scroll->AddChild(bottom_);

    middle_scrollable_ = FakePictureLayer::Create(&fake_content_layer_client_);
    middle_scrollable_->SetElementId(
        LayerIdToElementIdForTesting(middle_scrollable_->id()));
    middle_scrollable_->SetBounds(gfx::Size(100, 200));
    middle_scrollable_->SetIsDrawable(true);
    middle_scrollable_->SetHitTestable(true);
    CopyProperties(bottom_.get(), middle_scrollable_.get());
    CreateTransformNode(middle_scrollable_.get());
    CreateScrollNode(middle_scrollable_.get(), gfx::Size(100, 100));
    outer_scroll->AddChild(middle_scrollable_);

    top_ = FakePictureLayer::Create(&fake_content_layer_client_);
    top_->SetElementId(LayerIdToElementIdForTesting(top_->id()));
    top_->SetBounds(gfx::Size(100, 100));
    top_->SetNonFastScrollableRegion(Region(gfx::Rect(0, 0, 50, 50)));
    top_->SetHitTestable(true);
    CopyProperties(middle_scrollable_.get(), top_.get());
    outer_scroll->AddChild(top_);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    if (TestEnded())
      return;

    ScrollNode* scroll_node =
        impl->active_tree()->property_trees()->scroll_tree.Node(
            middle_scrollable_->scroll_tree_index());

    // The top-left hit should immediately hit the top layer's non-fast region.
    {
      auto status = impl->GetInputHandler().ScrollBegin(
          BeginState(gfx::Point(20, 20), gfx::Vector2dF(0, 1)).get(),
          ui::ScrollInputType::kTouchscreen);
      if (base::FeatureList::IsEnabled(features::kScrollUnification)) {
        // Hitting a non fast region should request a hit test from the main
        // thread.
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        EXPECT_TRUE(status.needs_main_thread_hit_test);
        impl->GetInputHandler().ScrollEnd();
      } else {
        // Prior to scroll unification, this forces scrolling to the main
        // thread.
        EXPECT_EQ(ScrollThread::SCROLL_ON_MAIN_THREAD, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
                  status.main_thread_scrolling_reasons);
      }
    }

    // The top-right hit should hit the top layer but not the non-fast region
    // so the scroll can be handled without involving the main thread.
    {
      InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
          BeginState(gfx::Point(80, 20), gfx::Vector2dF(0, 1)).get(),
          ui::ScrollInputType::kTouchscreen);
      if (base::FeatureList::IsEnabled(features::kScrollUnification)) {
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        EXPECT_FALSE(status.needs_main_thread_hit_test);
      } else {
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
                  status.main_thread_scrolling_reasons);
      }
      EXPECT_EQ(scroll_node, impl->CurrentlyScrollingNode());
      impl->GetInputHandler().ScrollEnd();
    }

    // The bottom-right should hit the bottom layer's non-fast region.
    {
      InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
          BeginState(gfx::Point(80, 80), gfx::Vector2dF(0, 1)).get(),
          ui::ScrollInputType::kTouchscreen);
      if (base::FeatureList::IsEnabled(features::kScrollUnification)) {
        // Even though the point intersects a non-fast region, the first hit
        // layer is scrollable from the compositor thread so no need to involve
        // the main thread.
        EXPECT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
        EXPECT_FALSE(status.needs_main_thread_hit_test);
        EXPECT_EQ(scroll_node, impl->CurrentlyScrollingNode());
        impl->GetInputHandler().ScrollEnd();
      } else {
        // Though the middle layer is a composited scroller and is hit first, we
        // cannot do a fast scroll because an ancestor on the scroll chain has
        // hit a non-fast region.
        EXPECT_EQ(ScrollThread::SCROLL_ON_MAIN_THREAD, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
                  status.main_thread_scrolling_reasons);
      }
    }

    EndTest();
  }

 private:
  FakeContentLayerClient fake_content_layer_client_;
  scoped_refptr<Layer> bottom_;
  scoped_refptr<Layer> middle_scrollable_;
  scoped_refptr<Layer> top_;
};

SINGLE_THREAD_TEST_F(NonScrollingNonFastScrollableRegion);

// This test verifies that scrolling in non layer list mode (used by UI
// compositor) is always "compositor scrolled", i.e. property trees are mutated
// and the updated layers redrawn.  This test intentionally doesn't inherit
// from LayerTreeHostScrollTest since that enables LayerLists.
class UnifiedScrollingRepaintOnScroll : public LayerTreeTest {
 public:
  UnifiedScrollingRepaintOnScroll() {
    scoped_feature_list.InitAndEnableFeature(features::kScrollUnification);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void SetupTree() override {
    LayerTreeTest::SetupTree();

    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetScrollable(gfx::Size(10, 10));
    layer_->SetBounds(gfx::Size(100, 100));
    layer_->SetIsDrawable(true);
    layer_->SetHitTestable(true);
    layer_->SetElementId(LayerIdToElementIdForTesting(layer_->id()));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (is_done_)
      return;
    is_done_ = true;
    EndTest();

    TransformTree& transform_tree =
        impl->active_tree()->property_trees()->transform_tree;
    ASSERT_FALSE(transform_tree.needs_update());

    // Perform a scroll over our FakePictureLayer.
    {
      InputHandler::ScrollStatus status = impl->GetInputHandler().ScrollBegin(
          BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 10)).get(),
          ui::ScrollInputType::kTouchscreen);

      ASSERT_EQ(ScrollThread::SCROLL_ON_IMPL_THREAD, status.thread);
      ASSERT_EQ(layer_->scroll_tree_index(),
                impl->CurrentlyScrollingNode()->id);

      impl->GetInputHandler().ScrollUpdate(
          UpdateState(gfx::Point(), gfx::Vector2dF(0, 10)).get());
      impl->GetInputHandler().ScrollEnd();
    }

    // All scrolling in non-layer-list mode (i.e. UI compositor) should be
    // "compositor" scrolling so it should mutate the property tree and redraw,
    // rather than relying on an update from the main thread.
    ASSERT_TRUE(transform_tree.needs_update());
  }

 private:
  bool is_done_ = false;
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
  base::test::ScopedFeatureList scoped_feature_list;
};

MULTI_THREAD_TEST_F(UnifiedScrollingRepaintOnScroll);

}  // namespace
}  // namespace cc
