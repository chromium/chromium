// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_painted_scrollbar_layer.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::_;

namespace cc {

namespace {

class PaintedScrollbarLayerTest : public testing::Test {
 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::kMain);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
  }

  FakeLayerTreeHostClient fake_client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
};

class MockScrollbar : public FakeScrollbar {
 public:
  MockScrollbar() {
    set_should_paint(true);
    set_has_thumb(true);
    set_is_overlay(false);
  }
  MOCK_METHOD3(PaintPart,
               void(PaintCanvas* canvas,
                    ScrollbarPart part,
                    const gfx::Rect& rect));

 private:
  ~MockScrollbar() override = default;
};

TEST_F(PaintedScrollbarLayerTest, NeedsPaint) {
  auto scrollbar = base::MakeRefCounted<MockScrollbar>();
  scoped_refptr<PaintedScrollbarLayer> scrollbar_layer =
      PaintedScrollbarLayer::Create(scrollbar);

  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host_->SetRootLayer(scrollbar_layer);
  UpdateDrawProperties(layer_tree_host_.get());

  EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());

  // Request no paint, but expect them to be painted because they have not
  // yet been initialized.
  scrollbar->set_needs_repaint_thumb(false);
  scrollbar->set_needs_repaint_track(false);
  EXPECT_CALL(*scrollbar, PaintPart(_, ScrollbarPart::kThumb, _)).Times(1);
  EXPECT_CALL(*scrollbar,
              PaintPart(_, ScrollbarPart::kTrackButtonsTickmarks, _))
      .Times(1);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());

  // The next update will paint nothing because the first update caused a paint.
  EXPECT_CALL(*scrollbar, PaintPart(_, ScrollbarPart::kThumb, _)).Times(0);
  EXPECT_CALL(*scrollbar,
              PaintPart(_, ScrollbarPart::kTrackButtonsTickmarks, _))
      .Times(0);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());

  // Enable the thumb.
  EXPECT_CALL(*scrollbar, PaintPart(_, ScrollbarPart::kThumb, _)).Times(1);
  EXPECT_CALL(*scrollbar,
              PaintPart(_, ScrollbarPart::kTrackButtonsTickmarks, _))
      .Times(0);
  scrollbar->set_needs_repaint_thumb(true);
  scrollbar->set_needs_repaint_track(false);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());

  // Enable the track.
  EXPECT_CALL(*scrollbar, PaintPart(_, ScrollbarPart::kThumb, _)).Times(0);
  EXPECT_CALL(*scrollbar,
              PaintPart(_, ScrollbarPart::kTrackButtonsTickmarks, _))
      .Times(1);
  scrollbar->set_needs_repaint_thumb(false);
  scrollbar->set_needs_repaint_track(true);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());
}

TEST_F(PaintedScrollbarLayerTest, InternalContentBounds) {
  auto scrollbar = base::MakeRefCounted<FakeScrollbar>();
  auto scrollbar_layer = PaintedScrollbarLayer::Create(scrollbar);
  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetBounds(gfx::Size(10, 100));

  layer_tree_host_->SetRootLayer(scrollbar_layer);
  UpdateDrawProperties(layer_tree_host_.get());
  EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());
  scrollbar_layer->Update();
  EXPECT_EQ(gfx::Size(10, 100), scrollbar_layer->internal_content_bounds());

  layer_tree_host_->SetViewportRectAndScale(
      layer_tree_host_->device_viewport_rect(), 2.0f,
      layer_tree_host_->local_surface_id_from_parent());
  UpdateDrawProperties(layer_tree_host_.get());
  scrollbar_layer->Update();
  EXPECT_EQ(gfx::Size(20, 200), scrollbar_layer->internal_content_bounds());

  layer_tree_host_->SetViewportRectAndScale(
      layer_tree_host_->device_viewport_rect(), 0.1f,
      layer_tree_host_->local_surface_id_from_parent());
  UpdateDrawProperties(layer_tree_host_.get());
  scrollbar_layer->Update();
  EXPECT_EQ(gfx::Size(10, 100), scrollbar_layer->internal_content_bounds());
}

}  // namespace
}  // namespace cc
