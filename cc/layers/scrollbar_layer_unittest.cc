// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_map>

#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_painted_scrollbar_layer.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeResourceTrackingUIResourceManager : public UIResourceManager {
 public:
  FakeResourceTrackingUIResourceManager()
      : next_id_(1),
        total_ui_resource_created_(0),
        total_ui_resource_deleted_(0) {}
  ~FakeResourceTrackingUIResourceManager() override = default;

  UIResourceId CreateUIResource(UIResourceClient* content) override {
    total_ui_resource_created_++;
    UIResourceId nid = next_id_++;
    ui_resource_bitmap_map_.insert(
        std::make_pair(nid, content->GetBitmap(nid, false)));
    return nid;
  }

  // Deletes a UI resource.  May safely be called more than once.
  void DeleteUIResource(UIResourceId id) override {
    auto iter = ui_resource_bitmap_map_.find(id);
    if (iter != ui_resource_bitmap_map_.end()) {
      ui_resource_bitmap_map_.erase(iter);
      total_ui_resource_deleted_++;
    }
  }

  size_t UIResourceCount() { return ui_resource_bitmap_map_.size(); }
  int TotalUIResourceDeleted() { return total_ui_resource_deleted_; }
  int TotalUIResourceCreated() { return total_ui_resource_created_; }

  gfx::Size ui_resource_size(UIResourceId id) {
    auto iter = ui_resource_bitmap_map_.find(id);
    if (iter != ui_resource_bitmap_map_.end())
      return iter->second.GetSize();
    return gfx::Size();
  }

  UIResourceBitmap* ui_resource_bitmap(UIResourceId id) {
    auto iter = ui_resource_bitmap_map_.find(id);
    if (iter != ui_resource_bitmap_map_.end())
      return &iter->second;
    return nullptr;
  }

 private:
  using UIResourceBitmapMap =
      std::unordered_map<UIResourceId, UIResourceBitmap>;
  UIResourceBitmapMap ui_resource_bitmap_map_;

  StubLayerTreeHostSingleThreadClient single_thread_client_;
  int next_id_;
  int total_ui_resource_created_;
  int total_ui_resource_deleted_;
};

class BaseScrollbarLayerTest : public testing::Test {
 public:
  explicit BaseScrollbarLayerTest(
      LayerTreeSettings::ScrollbarAnimator animator) {
    layer_tree_settings_.single_thread_proxy_scheduler = false;
    layer_tree_settings_.use_zero_copy = true;
    layer_tree_settings_.scrollbar_animator = animator;
    layer_tree_settings_.scrollbar_fade_delay =
        base::TimeDelta::FromMilliseconds(20);
    layer_tree_settings_.scrollbar_fade_duration =
        base::TimeDelta::FromMilliseconds(20);

    scrollbar_layer_id_ = -1;

    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

    LayerTreeHost::InitParams params;
    params.client = &fake_client_;
    params.settings = &layer_tree_settings_;
    params.task_graph_runner = &task_graph_runner_;
    params.mutator_host = animation_host_.get();

    std::unique_ptr<FakeResourceTrackingUIResourceManager>
        fake_ui_resource_manager =
            std::make_unique<FakeResourceTrackingUIResourceManager>();
    fake_ui_resource_manager_ = fake_ui_resource_manager.get();

    layer_tree_host_ = std::make_unique<FakeLayerTreeHost>(
        &fake_client_, std::move(params), CompositorMode::SINGLE_THREADED);
    layer_tree_host_->SetUIResourceManagerForTesting(
        std::move(fake_ui_resource_manager));
    layer_tree_host_->InitializeSingleThreaded(
        &single_thread_client_, base::ThreadTaskRunnerHandle::Get());
    layer_tree_host_->SetVisible(true);
    fake_client_.SetLayerTreeHost(layer_tree_host_.get());
  }

  LayerImpl* LayerImplForScrollAreaAndScrollbar(
      FakeLayerTreeHost* host,
      std::unique_ptr<Scrollbar> scrollbar,
      bool reverse_order,
      bool use_solid_color_scrollbar,
      int thumb_thickness,
      int track_start) {
    scoped_refptr<Layer> layer_tree_root = Layer::Create();
    scoped_refptr<Layer> child1 = Layer::Create();
    scoped_refptr<Layer> child2;
    if (use_solid_color_scrollbar) {
      const bool kIsLeftSideVerticalScrollbar = false;
      child2 = SolidColorScrollbarLayer::Create(
          scrollbar->Orientation(), thumb_thickness, track_start,
          kIsLeftSideVerticalScrollbar, child1->element_id());
    } else {
      child2 = PaintedScrollbarLayer::Create(std::move(scrollbar),
                                             child1->element_id());
    }
    layer_tree_root->AddChild(child1);
    layer_tree_root->InsertChild(child2, reverse_order ? 0 : 1);
    scrollbar_layer_id_ = reverse_order ? child1->id() : child2->id();
    host->SetRootLayer(layer_tree_root);
    host->BuildPropertyTreesForTesting();
    return host->CommitAndCreateLayerImplTree();
  }

 protected:
  FakeResourceTrackingUIResourceManager* fake_ui_resource_manager_;
  FakeLayerTreeHostClient fake_client_;
  StubLayerTreeHostSingleThreadClient single_thread_client_;
  TestTaskGraphRunner task_graph_runner_;
  LayerTreeSettings layer_tree_settings_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  int scrollbar_layer_id_;
};

class ScrollbarLayerTest : public BaseScrollbarLayerTest {
 public:
  ScrollbarLayerTest()
      : BaseScrollbarLayerTest(LayerTreeSettings::ANDROID_OVERLAY) {}
};

class AuraScrollbarLayerTest : public BaseScrollbarLayerTest {
 public:
  AuraScrollbarLayerTest()
      : BaseScrollbarLayerTest(LayerTreeSettings::AURA_OVERLAY) {}
};

class FakePaintedOverlayScrollbar : public FakeScrollbar {
 public:
  FakePaintedOverlayScrollbar() : FakeScrollbar(true, true, true) {}
  bool UsesNinePatchThumbResource() const override { return true; }
  gfx::Size NinePatchThumbCanvasSize() const override {
    return gfx::Size(3, 3);
  }
  gfx::Rect NinePatchThumbAperture() const override {
    return gfx::Rect(1, 1, 1, 1);
  }
};

// Test that a painted overlay scrollbar will repaint and recrate its resource
// after its been disposed, even if Blink doesn't think it requires a repaint.
// crbug.com/704656.
TEST_F(ScrollbarLayerTest, RepaintOverlayWhenResourceDisposed) {
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> content_layer = Layer::Create();
  std::unique_ptr<FakePaintedOverlayScrollbar> scrollbar(
      new FakePaintedOverlayScrollbar);
  FakePaintedOverlayScrollbar* fake_scrollbar = scrollbar.get();
  scoped_refptr<PaintedOverlayScrollbarLayer> scrollbar_layer =
      PaintedOverlayScrollbarLayer::Create(std::move(scrollbar),
                                           layer_tree_root->element_id());

  // Setup.
  {
    layer_tree_root->AddChild(content_layer);
    layer_tree_root->AddChild(scrollbar_layer);
    layer_tree_host_->SetRootLayer(layer_tree_root);
    scrollbar_layer->SetIsDrawable(true);
    scrollbar_layer->SetBounds(gfx::Size(100, 100));
    layer_tree_root->SetBounds(gfx::Size(100, 200));
    content_layer->SetBounds(gfx::Size(100, 200));
  }

  // First call to update should create a resource. The scrollbar itself thinks
  // it needs a repaint.
  {
    fake_scrollbar->set_needs_paint_thumb(true);
    EXPECT_EQ(0u, fake_ui_resource_manager_->UIResourceCount());
    EXPECT_TRUE(scrollbar_layer->Update());
    EXPECT_EQ(1u, fake_ui_resource_manager_->UIResourceCount());
  }

  // Now the scrollbar has been painted and nothing else has changed, calling
  // Update() shouldn't have an effect.
  {
    fake_scrollbar->set_needs_paint_thumb(false);
    EXPECT_FALSE(scrollbar_layer->Update());
    EXPECT_EQ(1u, fake_ui_resource_manager_->UIResourceCount());
  }

  // Detach and reattach the LayerTreeHost (this can happen during tree
  // reconstruction). This should cause the UIResource for the scrollbar to be
  // disposed but the scrollbar itself hasn't changed so it reports that no
  // repaint is needed. An Update should cause us to recreate the resource
  // though.
  {
    scrollbar_layer->SetLayerTreeHost(nullptr);
    scrollbar_layer->SetLayerTreeHost(layer_tree_host_.get());
    EXPECT_EQ(0u, fake_ui_resource_manager_->UIResourceCount());
    EXPECT_TRUE(scrollbar_layer->Update());
    EXPECT_EQ(1u, fake_ui_resource_manager_->UIResourceCount());
  }
}

TEST_F(ScrollbarLayerTest, ShouldScrollNonOverlayOnMainThread) {
  // Create and attach a non-overlay scrollbar.
  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar);
  LayerImpl* layer_impl_tree_root = LayerImplForScrollAreaAndScrollbar(
      layer_tree_host_.get(), std::move(scrollbar), false, false, 0, 0);
  PaintedScrollbarLayerImpl* scrollbar_layer_impl =
      static_cast<PaintedScrollbarLayerImpl*>(
          layer_impl_tree_root->layer_tree_impl()->LayerById(
              scrollbar_layer_id_));
  ScrollTree& scroll_tree =
      layer_impl_tree_root->layer_tree_impl()->property_trees()->scroll_tree;
  ScrollNode* scroll_node =
      scroll_tree.Node(scrollbar_layer_impl->scroll_tree_index());

  // When the scrollbar is not an overlay scrollbar, the scroll should be
  // responded to on the main thread as the compositor does not yet implement
  // scrollbar scrolling.
  InputHandler::ScrollStatus status = layer_tree_host_->host_impl()->TryScroll(
      gfx::PointF(), InputHandler::TOUCHSCREEN, scroll_tree, scroll_node);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kScrollbarScrolling,
            status.main_thread_scrolling_reasons);

  // Create and attach an overlay scrollbar.
  scrollbar.reset(new FakeScrollbar(false, false, true));

  layer_impl_tree_root = LayerImplForScrollAreaAndScrollbar(
      layer_tree_host_.get(), std::move(scrollbar), false, false, 0, 0);
  scrollbar_layer_impl = static_cast<PaintedScrollbarLayerImpl*>(
      layer_impl_tree_root->layer_tree_impl()->LayerById(scrollbar_layer_id_));
  scroll_tree =
      layer_impl_tree_root->layer_tree_impl()->property_trees()->scroll_tree;
  scroll_node = scroll_tree.Node(scrollbar_layer_impl->scroll_tree_index());

  // The user shouldn't be able to drag an overlay scrollbar and the scroll
  // may be handled in the compositor.
  status = layer_tree_host_->host_impl()->TryScroll(
      gfx::PointF(), InputHandler::TOUCHSCREEN, scroll_tree, scroll_node);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollable,
            status.main_thread_scrolling_reasons);
}

class FakeNinePatchScrollbar : public FakeScrollbar {
 public:
  bool UsesNinePatchThumbResource() const override { return true; }
};

TEST_F(ScrollbarLayerTest, ScrollElementIdPushedAcrossCommit) {
  std::unique_ptr<Scrollbar> scrollbar1(new FakeScrollbar);
  std::unique_ptr<Scrollbar> scrollbar2(new FakeNinePatchScrollbar);
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> layer_a = Layer::Create();
  scoped_refptr<Layer> layer_b = Layer::Create();
  layer_a->SetElementId(LayerIdToElementIdForTesting(layer_a->id()));
  layer_b->SetElementId(LayerIdToElementIdForTesting(layer_b->id()));

  scoped_refptr<PaintedScrollbarLayer> painted_scrollbar_layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar1),
                                    layer_a->element_id());
  scoped_refptr<PaintedOverlayScrollbarLayer> painted_overlay_scrollbar_layer =
      PaintedOverlayScrollbarLayer::Create(std::move(scrollbar2),
                                           layer_a->element_id());
  scoped_refptr<SolidColorScrollbarLayer> solid_color_scrollbar_layer =
      SolidColorScrollbarLayer::Create(VERTICAL, 1, 1, false,
                                       layer_a->element_id());

  layer_tree_host_->SetRootLayer(layer_tree_root);
  layer_tree_root->AddChild(layer_a);
  layer_tree_root->AddChild(layer_b);
  layer_tree_root->AddChild(painted_scrollbar_layer);
  layer_tree_root->AddChild(painted_overlay_scrollbar_layer);
  layer_tree_root->AddChild(solid_color_scrollbar_layer);

  layer_tree_host_->UpdateLayers();
  LayerImpl* layer_impl_tree_root =
      layer_tree_host_->CommitAndCreateLayerImplTree();

  ScrollbarLayerImplBase* painted_scrollbar_layer_impl =
      static_cast<ScrollbarLayerImplBase*>(
          layer_impl_tree_root->layer_tree_impl()->LayerById(
              painted_scrollbar_layer->id()));
  ScrollbarLayerImplBase* painted_overlay_scrollbar_layer_impl =
      static_cast<ScrollbarLayerImplBase*>(
          layer_impl_tree_root->layer_tree_impl()->LayerById(
              painted_overlay_scrollbar_layer->id()));
  ScrollbarLayerImplBase* solid_color_scrollbar_layer_impl =
      static_cast<ScrollbarLayerImplBase*>(
          layer_impl_tree_root->layer_tree_impl()->LayerById(
              solid_color_scrollbar_layer->id()));

  ASSERT_EQ(painted_scrollbar_layer_impl->scroll_element_id_,
            layer_a->element_id());
  ASSERT_EQ(painted_overlay_scrollbar_layer_impl->scroll_element_id_,
            layer_a->element_id());
  ASSERT_EQ(solid_color_scrollbar_layer_impl->scroll_element_id_,
            layer_a->element_id());

  painted_scrollbar_layer->SetScrollElementId(layer_b->element_id());
  painted_overlay_scrollbar_layer->SetScrollElementId(layer_b->element_id());
  solid_color_scrollbar_layer->SetScrollElementId(layer_b->element_id());

  ASSERT_TRUE(layer_tree_host_->needs_commit());

  {
    DebugScopedSetImplThread scoped_impl_thread(
        layer_tree_host_->GetTaskRunnerProvider());
    layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_->host_impl());
  }

  EXPECT_EQ(painted_scrollbar_layer_impl->scroll_element_id_,
            layer_b->element_id());
  EXPECT_EQ(painted_overlay_scrollbar_layer_impl->scroll_element_id_,
            layer_b->element_id());
  EXPECT_EQ(solid_color_scrollbar_layer_impl->scroll_element_id_,
            layer_b->element_id());
}

TEST_F(ScrollbarLayerTest, ScrollOffsetSynchronization) {
  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar);
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));
  scoped_refptr<Layer> content_layer = Layer::Create();
  scoped_refptr<Layer> scrollbar_layer = PaintedScrollbarLayer::Create(
      std::move(scrollbar), scroll_layer->element_id());

  // Choose bounds to give max_scroll_offset = (30, 50).
  layer_tree_root->SetBounds(gfx::Size(70, 150));
  scroll_layer->SetScrollOffset(gfx::ScrollOffset(10, 20));
  scroll_layer->SetBounds(gfx::Size(100, 200));
  scroll_layer->SetScrollable(gfx::Size(70, 150));
  content_layer->SetBounds(gfx::Size(100, 200));

  layer_tree_host_->SetRootLayer(layer_tree_root);
  layer_tree_root->AddChild(scroll_layer);
  scroll_layer->AddChild(content_layer);
  layer_tree_root->AddChild(scrollbar_layer);

  layer_tree_host_->UpdateLayers();
  LayerImpl* layer_impl_tree_root =
      layer_tree_host_->CommitAndCreateLayerImplTree();

  ScrollbarLayerImplBase* cc_scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(
          layer_impl_tree_root->layer_tree_impl()->LayerById(
              scrollbar_layer->id()));
  layer_impl_tree_root->layer_tree_impl()->UpdateScrollbarGeometries();

  EXPECT_EQ(10.f, cc_scrollbar_layer->current_pos());
  EXPECT_EQ(30, cc_scrollbar_layer->scroll_layer_length() -
                    cc_scrollbar_layer->clip_layer_length());

  layer_tree_root->SetBounds(gfx::Size(700, 1500));
  scroll_layer->SetScrollable(gfx::Size(700, 1500));
  scroll_layer->SetBounds(gfx::Size(1000, 2000));
  scroll_layer->SetScrollOffset(gfx::ScrollOffset(100, 200));
  content_layer->SetBounds(gfx::Size(1000, 2000));

  layer_tree_host_->UpdateLayers();
  layer_impl_tree_root = layer_tree_host_->CommitAndCreateLayerImplTree();
  layer_impl_tree_root->layer_tree_impl()->UpdateScrollbarGeometries();

  EXPECT_EQ(100.f, cc_scrollbar_layer->current_pos());
  EXPECT_EQ(300, cc_scrollbar_layer->scroll_layer_length() -
                     cc_scrollbar_layer->clip_layer_length());

  LayerImpl* scroll_layer_impl =
      layer_impl_tree_root->layer_tree_impl()->LayerById(scroll_layer->id());
  scroll_layer_impl->ScrollBy(gfx::Vector2d(12, 34));
  layer_impl_tree_root->layer_tree_impl()->UpdateScrollbarGeometries();

  EXPECT_EQ(112.f, cc_scrollbar_layer->current_pos());
  EXPECT_EQ(300, cc_scrollbar_layer->scroll_layer_length() -
                     cc_scrollbar_layer->clip_layer_length());
}

#define UPDATE_AND_EXTRACT_LAYER_POINTERS()                                    \
  do {                                                                         \
    scrollbar_layer->UpdateInternalContentScale();                             \
    scrollbar_layer->UpdateThumbAndTrackGeometry();                            \
    root_layer_impl = layer_tree_host_->CommitAndCreateLayerImplTree();        \
    root_layer_impl->layer_tree_impl()->UpdateScrollbarGeometries();           \
    scrollbar_layer_impl = static_cast<PaintedScrollbarLayerImpl*>(            \
        root_layer_impl->layer_tree_impl()->LayerById(scrollbar_layer->id())); \
  } while (false)

TEST_F(ScrollbarLayerTest, UpdatePropertiesOfScrollBarWhenThumbRemoved) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  scoped_refptr<Layer> content_layer = Layer::Create();
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer =
      FakePaintedScrollbarLayer::Create(false, true, root_layer->element_id());

  // Give the root layer a size that will result in MaxScrollOffset = (80, 0).
  root_layer->SetScrollable(gfx::Size(20, 50));
  root_layer->SetBounds(gfx::Size(100, 50));
  content_layer->SetBounds(gfx::Size(100, 50));

  layer_tree_host_->SetRootLayer(root_layer);
  root_layer->AddChild(content_layer);
  root_layer->AddChild(scrollbar_layer);

  root_layer->SetScrollOffset(gfx::ScrollOffset(0, 0));
  scrollbar_layer->SetBounds(gfx::Size(70, 10));
  scrollbar_layer->SetScrollElementId(root_layer->element_id());
  scrollbar_layer->fake_scrollbar()->set_location(gfx::Point(20, 10));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 10, 50, 10));
  scrollbar_layer->fake_scrollbar()->set_thumb_thickness(10);
  scrollbar_layer->fake_scrollbar()->set_thumb_length(4);
  LayerImpl* root_layer_impl = nullptr;
  PaintedScrollbarLayerImpl* scrollbar_layer_impl = nullptr;

  layer_tree_host_->BuildPropertyTreesForTesting();
  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(10, 0, 4, 10).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  scrollbar_layer->fake_scrollbar()->set_has_thumb(false);

  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(10, 0, 0, 0).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());
}

TEST_F(ScrollbarLayerTest, ThumbRect) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  scoped_refptr<Layer> content_layer = Layer::Create();
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer =
      FakePaintedScrollbarLayer::Create(false, true, root_layer->element_id());

  root_layer->SetElementId(LayerIdToElementIdForTesting(root_layer->id()));
  // Give the root layer a size that will result in MaxScrollOffset = (80, 0).
  root_layer->SetScrollable(gfx::Size(20, 50));
  root_layer->SetBounds(gfx::Size(100, 50));
  content_layer->SetBounds(gfx::Size(100, 50));

  layer_tree_host_->SetRootLayer(root_layer);
  root_layer->AddChild(content_layer);
  root_layer->AddChild(scrollbar_layer);

  root_layer->SetScrollOffset(gfx::ScrollOffset(0, 0));
  scrollbar_layer->SetBounds(gfx::Size(70, 10));
  scrollbar_layer->SetScrollElementId(root_layer->element_id());
  scrollbar_layer->fake_scrollbar()->set_location(gfx::Point(20, 10));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 10, 50, 10));
  scrollbar_layer->fake_scrollbar()->set_thumb_thickness(10);
  scrollbar_layer->fake_scrollbar()->set_thumb_length(4);
  layer_tree_host_->UpdateLayers();
  LayerImpl* root_layer_impl = nullptr;
  PaintedScrollbarLayerImpl* scrollbar_layer_impl = nullptr;

  // Thumb is at the edge of the scrollbar (should be inset to
  // the start of the track within the scrollbar layer's
  // position).
  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(10, 0, 4, 10).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Under-scroll (thumb position should clamp and be unchanged).
  root_layer->SetScrollOffset(gfx::ScrollOffset(-5, 0));

  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(10, 0, 4, 10).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Over-scroll (thumb position should clamp on the far side).
  root_layer->SetScrollOffset(gfx::ScrollOffset(85, 0));
  layer_tree_host_->UpdateLayers();

  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(56, 0, 4, 10).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Change thumb thickness and length.
  scrollbar_layer->fake_scrollbar()->set_thumb_thickness(4);
  scrollbar_layer->fake_scrollbar()->set_thumb_length(6);

  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(54, 0, 6, 4).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Shrink the scrollbar layer to cover only the track.
  scrollbar_layer->SetBounds(gfx::Size(50, 10));
  scrollbar_layer->fake_scrollbar()->set_location(gfx::Point(30, 10));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 10, 50, 10));

  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(44, 0, 6, 4).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Shrink the track in the non-scrolling dimension so that it only covers the
  // middle third of the scrollbar layer (this does not affect the thumb
  // position).
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 12, 50, 6));

  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(44, 0, 6, 4).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());
}

TEST_F(ScrollbarLayerTest, ThumbRectForOverlayLeftSideVerticalScrollbar) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  // Create an overlay left side vertical scrollbar.
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer =
      FakePaintedScrollbarLayer::Create(false, true, VERTICAL, true, true,
                                        root_layer->element_id());
  root_layer->SetScrollable(gfx::Size(20, 50));
  root_layer->SetBounds(gfx::Size(50, 100));

  layer_tree_host_->SetRootLayer(root_layer);
  root_layer->AddChild(scrollbar_layer);

  root_layer->SetScrollOffset(gfx::ScrollOffset(0, 0));
  scrollbar_layer->SetBounds(gfx::Size(10, 20));
  scrollbar_layer->SetScrollElementId(root_layer->element_id());
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(0, 0, 10, 20));
  scrollbar_layer->fake_scrollbar()->set_thumb_thickness(10);
  scrollbar_layer->fake_scrollbar()->set_thumb_length(4);
  layer_tree_host_->UpdateLayers();
  LayerImpl* root_layer_impl = nullptr;
  PaintedScrollbarLayerImpl* scrollbar_layer_impl = nullptr;

  // Thumb is at the edge of the scrollbar (should be inset to
  // the start of the track within the scrollbar layer's
  // position).
  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 4).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Change thumb thickness scale factor.
  scrollbar_layer_impl->SetThumbThicknessScaleFactor(0.5);
  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  // For overlay scrollbars thumb_rect.width = thumb_thickness *
  // thumb_thickness_scale_factor.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 4).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());

  // Change thumb thickness and length.
  scrollbar_layer->fake_scrollbar()->set_thumb_thickness(4);
  scrollbar_layer->fake_scrollbar()->set_thumb_length(6);
  UPDATE_AND_EXTRACT_LAYER_POINTERS();
  // For left side vertical scrollbars thumb_rect.x = bounds.width() -
  // thumb_thickness.
  EXPECT_EQ(gfx::Rect(6, 0, 2, 6).ToString(),
            scrollbar_layer_impl->ComputeThumbQuadRect().ToString());
}

TEST_F(ScrollbarLayerTest, SolidColorDrawQuads) {
  const int kThumbThickness = 3;
  const int kTrackStart = 1;
  const int kTrackLength = 100;

  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));
  LayerImpl* layer_impl_tree_root = LayerImplForScrollAreaAndScrollbar(
      layer_tree_host_.get(), std::move(scrollbar), false, true,
      kThumbThickness, kTrackStart);
  ScrollbarLayerImplBase* scrollbar_layer_impl =
      static_cast<SolidColorScrollbarLayerImpl*>(
          layer_impl_tree_root->layer_tree_impl()->LayerById(
              scrollbar_layer_id_));
  scrollbar_layer_impl->SetBounds(gfx::Size(kTrackLength, kThumbThickness));
  scrollbar_layer_impl->SetCurrentPos(10.f);
  scrollbar_layer_impl->SetClipLayerLength(200 / 3.f);
  scrollbar_layer_impl->SetScrollLayerLength(100 + 200 / 3.f);

  // Thickness should be overridden to 3.
  {
    std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(render_pass.get(), &data);

    const auto& quads = render_pass->quad_list;
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR, quads.front()->material);
    EXPECT_EQ(gfx::Rect(6, 0, 39, 3), quads.front()->rect);
  }

  // For solid color scrollbars, position and size should reflect the
  // current viewport state.
  scrollbar_layer_impl->SetClipLayerLength(25.f);
  scrollbar_layer_impl->SetScrollLayerLength(125.f);
  {
    std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(render_pass.get(), &data);

    const auto& quads = render_pass->quad_list;
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR, quads.front()->material);
    EXPECT_EQ(gfx::Rect(8, 0, 19, 3), quads.front()->rect);
  }

  // We shouldn't attempt div-by-zero when the maximum is zero.
  scrollbar_layer_impl->SetCurrentPos(0.f);
  scrollbar_layer_impl->SetClipLayerLength(125.f);
  scrollbar_layer_impl->SetScrollLayerLength(125.f);
  {
    std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(render_pass.get(), &data);

    const auto& quads = render_pass->quad_list;
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR, quads.front()->material);
    EXPECT_EQ(gfx::Rect(1, 0, 98, 3), quads.front()->rect);
  }
}

TEST_F(ScrollbarLayerTest, LayerDrivenSolidColorDrawQuads) {
  const int kThumbThickness = 3;
  const int kTrackStart = 0;
  const int kTrackLength = 10;

  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2;
  const bool kIsLeftSideVerticalScrollbar = false;
  child2 = SolidColorScrollbarLayer::Create(
      scrollbar->Orientation(), kThumbThickness, kTrackStart,
      kIsLeftSideVerticalScrollbar, scroll_layer->element_id());
  scroll_layer->AddChild(child1);
  scroll_layer->InsertChild(child2, 1);
  layer_tree_root->AddChild(scroll_layer);
  layer_tree_host_->SetRootLayer(layer_tree_root);

  // Choose layer bounds to give max_scroll_offset = (8, 8).
  layer_tree_root->SetBounds(gfx::Size(2, 2));
  scroll_layer->SetScrollable(gfx::Size(2, 2));
  scroll_layer->SetBounds(gfx::Size(10, 10));

  layer_tree_host_->UpdateLayers();

  LayerImpl* layer_impl_tree_root =
      layer_tree_host_->CommitAndCreateLayerImplTree();
  LayerImpl* scroll_layer_impl =
      layer_impl_tree_root->layer_tree_impl()->LayerById(scroll_layer->id());

  auto* scrollbar_layer_impl = static_cast<ScrollbarLayerImplBase*>(
      scroll_layer_impl->layer_tree_impl()->LayerById(child2->id()));

  scroll_layer_impl->ScrollBy(gfx::Vector2dF(4.f, 0.f));

  scrollbar_layer_impl->SetBounds(gfx::Size(kTrackLength, kThumbThickness));
  scrollbar_layer_impl->SetCurrentPos(4.f);

  DCHECK(layer_tree_host_->active_tree()->ScrollbarGeometriesNeedUpdate());
  layer_tree_host_->active_tree()->UpdateScrollbarGeometries();

  {
    std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(render_pass.get(), &data);

    const auto& quads = render_pass->quad_list;
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR, quads.front()->material);
    EXPECT_EQ(gfx::Rect(3, 0, 3, 3), quads.front()->rect);
  }
}

TEST_F(ScrollbarLayerTest, ScrollbarLayerOpacity) {
  const int kThumbThickness = 3;
  const int kTrackStart = 0;

  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scroll_layer->SetElementId(ElementId(200));
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<SolidColorScrollbarLayer> scrollbar_layer;
  const bool kIsLeftSideVerticalScrollbar = false;
  scrollbar_layer = SolidColorScrollbarLayer::Create(
      scrollbar->Orientation(), kThumbThickness, kTrackStart,
      kIsLeftSideVerticalScrollbar, scroll_layer->element_id());
  scrollbar_layer->SetElementId(ElementId(300));
  scroll_layer->AddChild(child1);
  scroll_layer->InsertChild(scrollbar_layer, 1);
  layer_tree_root->AddChild(scroll_layer);
  layer_tree_host_->SetRootLayer(layer_tree_root);
  scrollbar_layer->SetScrollElementId(scroll_layer->element_id());

  // Choose layer bounds to give max_scroll_offset = (8, 8).
  layer_tree_root->SetBounds(gfx::Size(2, 2));
  scroll_layer->SetBounds(gfx::Size(10, 10));

  // Building property trees twice shouldn't change the size of
  // PropertyTrees::always_use_active_tree_opacity_effect_ids.
  layer_tree_host_->BuildPropertyTreesForTesting();
  layer_tree_host_->property_trees()->needs_rebuild = true;
  layer_tree_host_->BuildPropertyTreesForTesting();

  // A solid color scrollbar layer's opacity is initialized to 0 on main thread
  layer_tree_host_->UpdateLayers();
  EffectNode* node = layer_tree_host_->property_trees()->effect_tree.Node(
      scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 0.f);

  // This tests that the initial opacity(0) of the scrollbar gets pushed onto
  // the pending tree and then onto the active tree.
  LayerTreeHostImpl* host_impl = layer_tree_host_->host_impl();
  host_impl->CreatePendingTree();
  LayerImpl* layer_impl_tree_root =
      layer_tree_host_->CommitAndCreatePendingTree();
  LayerTreeImpl* layer_tree_impl = layer_impl_tree_root->layer_tree_impl();
  EXPECT_TRUE(layer_tree_impl->IsPendingTree());
  node = layer_tree_impl->property_trees()->effect_tree.Node(
      scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 0.f);
  host_impl->ActivateSyncTree();
  layer_tree_impl = host_impl->active_tree();
  node = layer_tree_impl->property_trees()->effect_tree.Node(
      scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 0.f);

  // This tests that activation does not change the opacity of scrollbar layer.
  ScrollbarLayerImplBase* scrollbar_layer_impl =
      static_cast<ScrollbarLayerImplBase*>(
          layer_tree_impl->LayerById(scrollbar_layer->id()));
  scrollbar_layer_impl->SetOverlayScrollbarLayerOpacityAnimated(0.25f);
  host_impl->CreatePendingTree();
  layer_impl_tree_root = layer_tree_host_->CommitAndCreatePendingTree();
  layer_tree_impl = layer_impl_tree_root->layer_tree_impl();
  EXPECT_TRUE(layer_tree_impl->IsPendingTree());
  node = layer_tree_impl->property_trees()->effect_tree.Node(
      scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 0.f);
  host_impl->ActivateSyncTree();
  layer_tree_impl = host_impl->active_tree();
  node = layer_tree_impl->property_trees()->effect_tree.Node(
      scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 0.25f);
}

TEST_F(ScrollbarLayerTest, ScrollbarLayerPushProperties) {
  // Pushing changed bounds of scroll layer can lead to calling
  // OnOpacityAnimated on scrollbar layer which means OnOpacityAnimated should
  // be independent of scrollbar layer's properties as scrollbar layer can push
  // its properties after scroll layer.
  const int kThumbThickness = 3;
  const int kTrackStart = 0;
  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> scrollbar_layer;
  const bool kIsLeftSideVerticalScrollbar = false;
  scrollbar_layer = SolidColorScrollbarLayer::Create(
      scrollbar->Orientation(), kThumbThickness, kTrackStart,
      kIsLeftSideVerticalScrollbar, scroll_layer->element_id());
  scroll_layer->AddChild(child1);
  scroll_layer->InsertChild(scrollbar_layer, 1);
  layer_tree_root->AddChild(scroll_layer);
  layer_tree_host_->SetRootLayer(layer_tree_root);

  layer_tree_root->SetBounds(gfx::Size(2, 2));
  scroll_layer->SetBounds(gfx::Size(10, 10));
  scroll_layer->SetScrollable(layer_tree_root->bounds());
  layer_tree_host_->UpdateLayers();
  LayerTreeHostImpl* host_impl = layer_tree_host_->host_impl();
  host_impl->CreatePendingTree();
  layer_tree_host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();
  EXPECT_TRUE(host_impl->ScrollbarAnimationControllerForElementId(
      scroll_layer->element_id()));

  scroll_layer->SetBounds(gfx::Size(20, 20));
  scroll_layer->ShowScrollbars();
  scroll_layer->SetForceRenderSurfaceForTesting(true);
  layer_tree_host_->UpdateLayers();
  host_impl->CreatePendingTree();
  layer_tree_host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();
  EffectNode* node =
      host_impl->active_tree()->property_trees()->effect_tree.Node(
          scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 1.f);
}

TEST_F(ScrollbarLayerTest, SubPixelCanScrollOrientation) {
  gfx::Size viewport_size(980, 980);

  LayerTestCommon::LayerImplTest impl;

  LayerImpl* scroll_layer = impl.AddChildToRoot<LayerImpl>();
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));

  const int kTrackStart = 0;
  const int kThumbThickness = 10;
  const bool kIsLeftSideVerticalScrollbar = false;
  const bool kIsOverlayScrollbar = false;

  SolidColorScrollbarLayerImpl* scrollbar_layer =
      impl.AddChild<SolidColorScrollbarLayerImpl>(
          scroll_layer, HORIZONTAL, kThumbThickness, kTrackStart,
          kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);

  scrollbar_layer->SetScrollElementId(scroll_layer->element_id());
  scroll_layer->SetScrollable(gfx::Size(980, 980));
  scroll_layer->SetBounds(gfx::Size(980, 980));

  impl.host_impl()->active_tree()->BuildPropertyTreesForTesting();
  DCHECK(impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());
  impl.host_impl()->active_tree()->UpdateScrollbarGeometries();

  impl.CalcDrawProps(viewport_size);

  // Fake clip layer length to scrollbar to mock rounding error.
  scrollbar_layer->SetClipLayerLength(979.999939f);
  impl.host_impl()->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_FALSE(scrollbar_layer->CanScrollOrientation());

  // Fake clip layer length to scrollable.
  scrollbar_layer->SetClipLayerLength(979.0f);
  impl.host_impl()->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_TRUE(scrollbar_layer->CanScrollOrientation());
}

TEST_F(ScrollbarLayerTest, LayerChangesAffectingScrollbarGeometries) {
  LayerTestCommon::LayerImplTest impl;

  LayerImpl* clip_layer = impl.AddChildToRoot<LayerImpl>();
  LayerImpl* scroll_layer = impl.AddChild<LayerImpl>(clip_layer);
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));

  // Make clip_layer the inner viewport container layer. This ensures the later
  // call to |SetViewportBoundsDelta| will be on a viewport layer.
  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.inner_viewport_container = clip_layer->id();
  impl.host_impl()->active_tree()->SetViewportLayersFromIds(viewport_ids);

  const int kTrackStart = 0;
  const int kThumbThickness = 10;
  const bool kIsLeftSideVerticalScrollbar = false;
  const bool kIsOverlayScrollbar = false;
  SolidColorScrollbarLayerImpl* scrollbar_layer =
      impl.AddChild<SolidColorScrollbarLayerImpl>(
          scroll_layer, HORIZONTAL, kThumbThickness, kTrackStart,
          kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
  scrollbar_layer->SetScrollElementId(scroll_layer->element_id());
  EXPECT_TRUE(impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());
  impl.host_impl()->active_tree()->UpdateScrollbarGeometries();

  scroll_layer->SetBounds(gfx::Size(900, 900));
  // If the scroll layer is not scrollable, the bounds do not affect scrollbar
  // geometries.
  EXPECT_FALSE(
      impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());

  scroll_layer->SetScrollable(gfx::Size(900, 900));
  EXPECT_TRUE(impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());
  impl.host_impl()->active_tree()->UpdateScrollbarGeometries();

  clip_layer->SetBounds(gfx::Size(900, 900));
  // The clip layer for scrolling is managed independently of the scroll
  // container bounds so changing the clip does not require an update.
  EXPECT_FALSE(
      impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());

  scroll_layer->SetBounds(gfx::Size(980, 980));
  // Changes to the bounds should also require an update.
  EXPECT_TRUE(impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());
  impl.host_impl()->active_tree()->UpdateScrollbarGeometries();

  clip_layer->SetViewportBoundsDelta(gfx::Vector2dF(1, 2));
  EXPECT_TRUE(impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());
  impl.host_impl()->active_tree()->UpdateScrollbarGeometries();

  // Not changing the current value should not require an update.
  scroll_layer->SetScrollable(gfx::Size(900, 900));
  clip_layer->SetBounds(gfx::Size(900, 900));
  scroll_layer->SetBounds(gfx::Size(980, 980));
  clip_layer->SetViewportBoundsDelta(gfx::Vector2dF(1, 2));
  EXPECT_TRUE(
      !impl.host_impl()->active_tree()->ScrollbarGeometriesNeedUpdate());
}

TEST_F(AuraScrollbarLayerTest, ScrollbarLayerCreateAfterSetScrollable) {
  // Scrollbar Layer can be created after SetScrollable is called and in a
  // separate commit. Ensure we do not missing the DidRequestShowFromMainThread
  // call.
  const int kThumbThickness = 3;
  const int kTrackStart = 0;

  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));
  scoped_refptr<Layer> child1 = Layer::Create();
  const bool kIsLeftSideVerticalScrollbar = false;

  scroll_layer->AddChild(child1);
  layer_tree_root->AddChild(scroll_layer);
  layer_tree_host_->SetRootLayer(layer_tree_root);

  layer_tree_root->SetBounds(gfx::Size(2, 2));
  scroll_layer->SetBounds(gfx::Size(10, 10));
  scroll_layer->SetScrollable(layer_tree_root->bounds());
  layer_tree_host_->UpdateLayers();
  LayerTreeHostImpl* host_impl = layer_tree_host_->host_impl();
  host_impl->CreatePendingTree();
  layer_tree_host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));
  scoped_refptr<Layer> scrollbar_layer = SolidColorScrollbarLayer::Create(
      scrollbar->Orientation(), kThumbThickness, kTrackStart,
      kIsLeftSideVerticalScrollbar, scroll_layer->element_id());
  scroll_layer->InsertChild(scrollbar_layer, 1);

  layer_tree_host_->UpdateLayers();
  host_impl->CreatePendingTree();
  layer_tree_host_->CommitAndCreatePendingTree();
  host_impl->ActivateSyncTree();

  EXPECT_TRUE(host_impl->ScrollbarAnimationControllerForElementId(
      scroll_layer->element_id()));
  EffectNode* node =
      host_impl->active_tree()->property_trees()->effect_tree.Node(
          scrollbar_layer->effect_tree_index());
  EXPECT_EQ(node->opacity, 1.f);
}

class ScrollbarLayerSolidColorThumbTest : public testing::Test {
 public:
  ScrollbarLayerSolidColorThumbTest() {
    LayerTreeSettings layer_tree_settings;
    host_impl_.reset(new FakeLayerTreeHostImpl(
        layer_tree_settings, &task_runner_provider_, &task_graph_runner_));

    const int kThumbThickness = 3;
    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;
    const bool kIsOverlayScrollbar = false;

    horizontal_scrollbar_layer_ =
        SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                             1,
                                             HORIZONTAL,
                                             kThumbThickness,
                                             kTrackStart,
                                             kIsLeftSideVerticalScrollbar,
                                             kIsOverlayScrollbar);
    vertical_scrollbar_layer_ =
        SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                             2,
                                             VERTICAL,
                                             kThumbThickness,
                                             kTrackStart,
                                             kIsLeftSideVerticalScrollbar,
                                             kIsOverlayScrollbar);
  }

 protected:
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<FakeLayerTreeHostImpl> host_impl_;
  std::unique_ptr<SolidColorScrollbarLayerImpl> horizontal_scrollbar_layer_;
  std::unique_ptr<SolidColorScrollbarLayerImpl> vertical_scrollbar_layer_;
};

TEST_F(ScrollbarLayerSolidColorThumbTest, SolidColorThumbLength) {
  horizontal_scrollbar_layer_->SetCurrentPos(0);

  // Simple case - one third of the scrollable area is visible, so the thumb
  // should be one third as long as the track.
  horizontal_scrollbar_layer_->SetClipLayerLength(5.f);
  horizontal_scrollbar_layer_->SetScrollLayerLength(15.f);
  horizontal_scrollbar_layer_->SetBounds(gfx::Size(100, 3));
  EXPECT_EQ(33, horizontal_scrollbar_layer_->ComputeThumbQuadRect().width());

  // The thumb's length should never be less than its thickness.
  horizontal_scrollbar_layer_->SetClipLayerLength(0.01f);
  horizontal_scrollbar_layer_->SetScrollLayerLength(15.f);
  horizontal_scrollbar_layer_->SetBounds(gfx::Size(100, 3));
  EXPECT_EQ(3, horizontal_scrollbar_layer_->ComputeThumbQuadRect().width());
}

TEST_F(ScrollbarLayerSolidColorThumbTest, SolidColorThumbPosition) {
  horizontal_scrollbar_layer_->SetBounds(gfx::Size(100, 3));
  horizontal_scrollbar_layer_->SetCurrentPos(0.f);
  horizontal_scrollbar_layer_->SetClipLayerLength(12.f);
  horizontal_scrollbar_layer_->SetScrollLayerLength(112.f);
  EXPECT_EQ(0, horizontal_scrollbar_layer_->ComputeThumbQuadRect().x());
  EXPECT_EQ(10, horizontal_scrollbar_layer_->ComputeThumbQuadRect().width());

  horizontal_scrollbar_layer_->SetCurrentPos(100);
  // The thumb is 10px long and the track is 100px, so the maximum thumb
  // position is 90px.
  EXPECT_EQ(90, horizontal_scrollbar_layer_->ComputeThumbQuadRect().x());

  horizontal_scrollbar_layer_->SetCurrentPos(80);
  // The scroll position is 80% of the maximum, so the thumb's position should
  // be at 80% of its maximum or 72px.
  EXPECT_EQ(72, horizontal_scrollbar_layer_->ComputeThumbQuadRect().x());
}

TEST_F(ScrollbarLayerSolidColorThumbTest, SolidColorThumbVerticalAdjust) {
  SolidColorScrollbarLayerImpl* layers[2] =
      { horizontal_scrollbar_layer_.get(), vertical_scrollbar_layer_.get() };
  for (size_t i = 0; i < 2; ++i) {
    layers[i]->SetCurrentPos(25.f);
    layers[i]->SetClipLayerLength(25.f);
    layers[i]->SetScrollLayerLength(125.f);
  }
  layers[0]->SetBounds(gfx::Size(100, 3));
  layers[1]->SetBounds(gfx::Size(3, 100));

  EXPECT_EQ(gfx::Rect(20, 0, 20, 3),
            horizontal_scrollbar_layer_->ComputeThumbQuadRect());
  EXPECT_EQ(gfx::Rect(0, 20, 3, 20),
            vertical_scrollbar_layer_->ComputeThumbQuadRect());

  horizontal_scrollbar_layer_->SetVerticalAdjust(10.f);
  vertical_scrollbar_layer_->SetVerticalAdjust(10.f);

  // The vertical adjustment factor has two effects:
  // 1.) Moves the horizontal scrollbar down
  // 2.) Increases the vertical scrollbar's effective track length which both
  // increases the thumb's length and its position within the track.
  EXPECT_EQ(gfx::Rect(20.f, 10.f, 20.f, 3.f),
            horizontal_scrollbar_layer_->ComputeThumbQuadRect());
  EXPECT_EQ(gfx::Rect(0.f, 22, 3.f, 22.f),
            vertical_scrollbar_layer_->ComputeThumbQuadRect());
}

class ScrollbarLayerTestResourceCreationAndRelease : public ScrollbarLayerTest {
 public:
  void TestResourceUpload(int num_updates,
                          size_t expected_resources,
                          int expected_created,
                          int expected_deleted,
                          bool use_solid_color_scrollbar) {
    std::unique_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, false));
    scoped_refptr<Layer> layer_tree_root = Layer::Create();
    scoped_refptr<Layer> content_layer = Layer::Create();
    scoped_refptr<Layer> scrollbar_layer;
    if (use_solid_color_scrollbar) {
      const int kThumbThickness = 3;
      const int kTrackStart = 0;
      const bool kIsLeftSideVerticalScrollbar = false;
      scrollbar_layer = SolidColorScrollbarLayer::Create(
          scrollbar->Orientation(), kThumbThickness, kTrackStart,
          kIsLeftSideVerticalScrollbar, layer_tree_root->element_id());
    } else {
      scrollbar_layer = PaintedScrollbarLayer::Create(
          std::move(scrollbar), layer_tree_root->element_id());
    }
    layer_tree_root->AddChild(content_layer);
    layer_tree_root->AddChild(scrollbar_layer);

    layer_tree_host_->SetRootLayer(layer_tree_root);

    scrollbar_layer->SetIsDrawable(true);
    scrollbar_layer->SetBounds(gfx::Size(100, 100));
    layer_tree_root->SetScrollable(gfx::Size(100, 200));
    layer_tree_root->SetScrollOffset(gfx::ScrollOffset(10, 20));
    layer_tree_root->SetBounds(gfx::Size(100, 200));
    content_layer->SetBounds(gfx::Size(100, 200));

    testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());

    for (int update_counter = 0; update_counter < num_updates; update_counter++)
      scrollbar_layer->Update();

    // A non-solid-color scrollbar should have requested two textures.
    EXPECT_EQ(expected_resources, fake_ui_resource_manager_->UIResourceCount());
    EXPECT_EQ(expected_created,
              fake_ui_resource_manager_->TotalUIResourceCreated());
    EXPECT_EQ(expected_deleted,
              fake_ui_resource_manager_->TotalUIResourceDeleted());

    testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }
};

TEST_F(ScrollbarLayerTestResourceCreationAndRelease, ResourceUpload) {
  bool use_solid_color_scrollbars = false;
  TestResourceUpload(0, 0, 0, 0, use_solid_color_scrollbars);
  int num_updates[3] = {1, 5, 10};
  int created = 0;
  int deleted = 0;
  for (int j = 0; j < 3; j++) {
    created += num_updates[j] * 2;
    deleted = created - 2;
    TestResourceUpload(num_updates[j], 2, created, deleted,
                       use_solid_color_scrollbars);
  }
}

TEST_F(ScrollbarLayerTestResourceCreationAndRelease,
       SolidColorNoResourceUpload) {
  bool use_solid_color_scrollbars = true;
  TestResourceUpload(0, 0, 0, 0, use_solid_color_scrollbars);
  TestResourceUpload(1, 0, 0, 0, use_solid_color_scrollbars);
}

TEST_F(ScrollbarLayerTestResourceCreationAndRelease, TestResourceUpdate) {
  gfx::Point scrollbar_location(0, 185);
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> content_layer = Layer::Create();
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer =
      FakePaintedScrollbarLayer::Create(false, true,
                                        layer_tree_root->element_id());

  layer_tree_root->AddChild(content_layer);
  layer_tree_root->AddChild(scrollbar_layer);

  layer_tree_host_->SetRootLayer(layer_tree_root);

  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetBounds(gfx::Size(100, 15));
  scrollbar_layer->SetPosition(gfx::PointF(scrollbar_location));
  layer_tree_root->SetBounds(gfx::Size(100, 200));
  content_layer->SetBounds(gfx::Size(100, 200));

  testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());

  size_t resource_count;
  int expected_created, expected_deleted;

  resource_count = 2;
  expected_created = 2;
  expected_deleted = 0;
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_NE(0, scrollbar_layer->track_resource_id());
  EXPECT_NE(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 0;
  expected_created = 2;
  expected_deleted = 2;
  scrollbar_layer->SetBounds(gfx::Size(0, 0));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(0, 0, 0, 0));
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_EQ(0, scrollbar_layer->track_resource_id());
  EXPECT_EQ(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 0;
  expected_created = 2;
  expected_deleted = 2;
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(0, 0, 0, 0));
  EXPECT_FALSE(scrollbar_layer->Update());
  EXPECT_EQ(0, scrollbar_layer->track_resource_id());
  EXPECT_EQ(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 2;
  expected_created = 4;
  expected_deleted = 2;
  scrollbar_layer->SetBounds(gfx::Size(100, 15));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 10, 50, 10));
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_NE(0, scrollbar_layer->track_resource_id());
  EXPECT_NE(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 1;
  expected_created = 5;
  expected_deleted = 4;
  scrollbar_layer->fake_scrollbar()->set_has_thumb(false);
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_NE(0, scrollbar_layer->track_resource_id());
  EXPECT_EQ(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 0;
  expected_created = 5;
  expected_deleted = 5;
  scrollbar_layer->SetBounds(gfx::Size(0, 0));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(0, 0, 0, 0));
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_EQ(0, scrollbar_layer->track_resource_id());
  EXPECT_EQ(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 2;
  expected_created = 7;
  expected_deleted = 5;
  scrollbar_layer->SetBounds(gfx::Size(100, 15));
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 10, 50, 10));
  scrollbar_layer->fake_scrollbar()->set_has_thumb(true);
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_NE(0, scrollbar_layer->track_resource_id());
  EXPECT_NE(0, scrollbar_layer->thumb_resource_id());

  resource_count = 2;
  expected_created = 9;
  expected_deleted = 7;
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(0, 0, 0, 0));
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_NE(0, scrollbar_layer->track_resource_id());
  EXPECT_NE(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  resource_count = 1;
  expected_created = 10;
  expected_deleted = 9;
  scrollbar_layer->fake_scrollbar()->set_track_rect(gfx::Rect(30, 10, 50, 10));
  scrollbar_layer->fake_scrollbar()->set_has_thumb(false);
  scrollbar_layer->SetBounds(gfx::Size(90, 15));
  EXPECT_TRUE(scrollbar_layer->Update());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());
  EXPECT_EQ(gfx::Size(90, 15), fake_ui_resource_manager_->ui_resource_size(
                                   scrollbar_layer->track_resource_id()));

  // Simulate commit to compositor thread.
  scrollbar_layer->PushPropertiesTo(
      scrollbar_layer->CreateLayerImpl(layer_tree_host_->active_tree()).get());

  EXPECT_FALSE(scrollbar_layer->Update());
  EXPECT_NE(0, scrollbar_layer->track_resource_id());
  EXPECT_EQ(0, scrollbar_layer->thumb_resource_id());
  EXPECT_EQ(resource_count, fake_ui_resource_manager_->UIResourceCount());
  EXPECT_EQ(expected_created,
            fake_ui_resource_manager_->TotalUIResourceCreated());
  EXPECT_EQ(expected_deleted,
            fake_ui_resource_manager_->TotalUIResourceDeleted());

  testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

class ScaledScrollbarLayerTestResourceCreation : public ScrollbarLayerTest {
 public:
  void TestResourceUpload(float test_scale) {
    gfx::Point scrollbar_location(0, 185);
    scoped_refptr<Layer> layer_tree_root = Layer::Create();
    scoped_refptr<Layer> content_layer = Layer::Create();
    scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer =
        FakePaintedScrollbarLayer::Create(false, true,
                                          layer_tree_root->element_id());

    layer_tree_root->AddChild(content_layer);
    layer_tree_root->AddChild(scrollbar_layer);

    layer_tree_host_->SetRootLayer(layer_tree_root);

    scrollbar_layer->SetIsDrawable(true);
    scrollbar_layer->SetBounds(gfx::Size(100, 15));
    scrollbar_layer->SetPosition(gfx::PointF(scrollbar_location));
    layer_tree_root->SetBounds(gfx::Size(100, 200));
    content_layer->SetBounds(gfx::Size(100, 200));

    EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());

    layer_tree_host_->SetViewportSizeAndScale(
        layer_tree_host_->device_viewport_size(), test_scale,
        layer_tree_host_->local_surface_id_from_parent(),
        layer_tree_host_->local_surface_id_allocation_time_from_parent());

    scrollbar_layer->Update();

    // Verify that we have not generated any content uploads that are larger
    // than their destination textures.

    gfx::Size track_size = fake_ui_resource_manager_->ui_resource_size(
        scrollbar_layer->track_resource_id());
    gfx::Size thumb_size = fake_ui_resource_manager_->ui_resource_size(
        scrollbar_layer->thumb_resource_id());

    EXPECT_LE(track_size.width(),
              scrollbar_layer->internal_content_bounds().width());
    EXPECT_LE(track_size.height(),
              scrollbar_layer->internal_content_bounds().height());
    EXPECT_LE(thumb_size.width(),
              scrollbar_layer->internal_content_bounds().width());
    EXPECT_LE(thumb_size.height(),
              scrollbar_layer->internal_content_bounds().height());
  }
};

TEST_F(ScaledScrollbarLayerTestResourceCreation, ScaledResourceUpload) {
  // Pick a test scale that moves the scrollbar's (non-zero) position to
  // a non-pixel-aligned location.
  TestResourceUpload(.041f);
  TestResourceUpload(1.41f);
  TestResourceUpload(4.1f);

  // Try something extreme to be larger than max texture size, and make it a
  // non-integer for funsies.
  scoped_refptr<viz::TestContextProvider> context =
      viz::TestContextProvider::Create();
  // Keep the max texture size reasonable so we don't OOM on low end devices
  // (crbug.com/642333).
  context->UnboundTestContextGL()->set_max_texture_size(512);
  context->BindToCurrentThread();
  int max_texture_size = 0;
  context->ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  EXPECT_EQ(512, max_texture_size);
  TestResourceUpload(max_texture_size / 9.9f);
}

class ScaledScrollbarLayerTestScaledRasterization : public ScrollbarLayerTest {
 public:
  void TestScale(const gfx::Rect& scrollbar_rect, float test_scale) {
    bool paint_during_update = true;
    bool has_thumb = false;
    scoped_refptr<Layer> layer_tree_root = Layer::Create();
    scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer =
        FakePaintedScrollbarLayer::Create(paint_during_update, has_thumb,
                                          layer_tree_root->element_id());

    layer_tree_root->AddChild(scrollbar_layer);

    layer_tree_host_->SetRootLayer(layer_tree_root);

    scrollbar_layer->SetBounds(scrollbar_rect.size());
    scrollbar_layer->SetPosition(gfx::PointF(scrollbar_rect.origin()));
    scrollbar_layer->fake_scrollbar()->set_location(scrollbar_rect.origin());
    scrollbar_layer->fake_scrollbar()->set_track_rect(scrollbar_rect);

    layer_tree_host_->SetViewportSizeAndScale(
        layer_tree_host_->device_viewport_size(), test_scale,
        layer_tree_host_->local_surface_id_from_parent(),
        layer_tree_host_->local_surface_id_allocation_time_from_parent());

    scrollbar_layer->Update();

    UIResourceBitmap* bitmap = fake_ui_resource_manager_->ui_resource_bitmap(
        scrollbar_layer->track_resource_id());

    DCHECK(bitmap);

    const SkColor* pixels =
        reinterpret_cast<const SkColor*>(bitmap->GetPixels());
    SkColor color = argb_to_skia(
        scrollbar_layer->fake_scrollbar()->paint_fill_color());
    int width = bitmap->GetSize().width();
    int height = bitmap->GetSize().height();

    // Make sure none of the corners of the bitmap were inadvertently clipped.
    EXPECT_EQ(color, pixels[0])
        << "Top left pixel doesn't match scrollbar color.";

    EXPECT_EQ(color, pixels[width - 1])
        << "Top right pixel doesn't match scrollbar color.";

    EXPECT_EQ(color, pixels[width * (height - 1)])
        << "Bottom left pixel doesn't match scrollbar color.";

    EXPECT_EQ(color, pixels[width * height - 1])
        << "Bottom right pixel doesn't match scrollbar color.";
  }

 protected:
  // On Android, Skia uses ABGR
  static SkColor argb_to_skia(SkColor c) {
      return (SkColorGetA(c) << SK_A32_SHIFT) |
             (SkColorGetR(c) << SK_R32_SHIFT) |
             (SkColorGetG(c) << SK_G32_SHIFT) |
             (SkColorGetB(c) << SK_B32_SHIFT);
  }
};

TEST_F(ScaledScrollbarLayerTestScaledRasterization, TestLostPrecisionInClip) {
  // Try rasterization at coordinates and scale that caused problematic
  // rounding and clipping errors.
  // Vertical Scrollbars.
  TestScale(gfx::Rect(1240, 0, 15, 1333), 2.7754839f);
  TestScale(gfx::Rect(1240, 0, 15, 677), 2.46677136f);

  // Horizontal Scrollbars.
  TestScale(gfx::Rect(0, 1240, 1333, 15), 2.7754839f);
  TestScale(gfx::Rect(0, 1240, 677, 15), 2.46677136f);
}

}  // namespace cc
