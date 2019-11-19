// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_overlay_scrollbar_layer.h"

#include "cc/animation/animation_host.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

class MockScrollbar : public FakeScrollbar {
 public:
  MockScrollbar() : FakeScrollbar(true, true, true) {}

  void PaintPart(PaintCanvas*, ScrollbarPart part, const gfx::Rect&) override {
    if (part == TRACK_BUTTONS_TICKMARKS)
      paint_tickmarks_called_ = true;
  }

  bool UsesNinePatchThumbResource() const override { return true; }

  gfx::Size NinePatchThumbCanvasSize() const override {
    return gfx::Size(10, 10);
  }

  bool PaintTickmarksCalled() { return paint_tickmarks_called_; }

  void SetPaintTickmarksCalled(bool called) {
    paint_tickmarks_called_ = called;
  }

 private:
  ~MockScrollbar() override = default;

  bool paint_tickmarks_called_ = false;
};

TEST(PaintedOverlayScrollbarLayerTest, PaintTickmarks) {
  FakeLayerTreeHostClient fake_client_;
  TestTaskGraphRunner task_graph_runner_;

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  auto layer_tree_host = FakeLayerTreeHost::Create(
      &fake_client_, &task_graph_runner_, animation_host.get());

  auto scrollbar = base::MakeRefCounted<MockScrollbar>();
  scrollbar->set_has_tickmarks(false);

  scoped_refptr<PaintedOverlayScrollbarLayer> scrollbar_layer =
      PaintedOverlayScrollbarLayer::Create(scrollbar);

  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host->SetRootLayer(scrollbar_layer);
  EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host.get());

  // Request no paint when initialization.
  scrollbar_layer->Update();
  EXPECT_FALSE(scrollbar->PaintTickmarksCalled());

  // The next update will paint nothing because still no tickmarks applied.
  scrollbar_layer->Update();
  EXPECT_FALSE(scrollbar->PaintTickmarksCalled());

  // Enable the tickmarks.
  scrollbar->set_has_tickmarks(true);
  scrollbar_layer->Update();
  EXPECT_TRUE(scrollbar->PaintTickmarksCalled());
  scrollbar->SetPaintTickmarksCalled(false);

  // Disable the tickmarks. No paint.
  scrollbar->set_has_tickmarks(false);
  scrollbar_layer->Update();
  EXPECT_FALSE(scrollbar->PaintTickmarksCalled());
}

}  // namespace
}  // namespace cc
