// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include "cc/animation/animation_host.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::_;

namespace cc {

namespace {

class MockScrollbar : public FakeScrollbar {
 public:
  MockScrollbar()
      : FakeScrollbar(/*paint*/ true,
                      /*has_thumb*/ true,
                      /*is_overlay*/ false) {}
  MOCK_METHOD3(PaintPart,
               void(PaintCanvas* canvas,
                    ScrollbarPart part,
                    const gfx::Rect& rect));

 private:
  ~MockScrollbar() override = default;
};

TEST(PaintedScrollbarLayerTest, NeedsPaint) {
  FakeLayerTreeHostClient fake_client_;
  TestTaskGraphRunner task_graph_runner_;

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  auto layer_tree_host = FakeLayerTreeHost::Create(
      &fake_client_, &task_graph_runner_, animation_host.get());

  auto scrollbar = base::MakeRefCounted<MockScrollbar>();
  scoped_refptr<PaintedScrollbarLayer> scrollbar_layer =
      PaintedScrollbarLayer::Create(scrollbar);

  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host->SetRootLayer(scrollbar_layer);
  UpdateDrawProperties(layer_tree_host.get());

  EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host.get());

  // Request no paint, but expect them to be painted because they have not
  // yet been initialized.
  scrollbar->set_needs_repaint_thumb(false);
  scrollbar->set_needs_repaint_track(false);
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(1);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK_BUTTONS_TICKMARKS, _)).Times(1);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());

  // The next update will paint nothing because the first update caused a paint.
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(0);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK_BUTTONS_TICKMARKS, _)).Times(0);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());

  // Enable the thumb.
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(1);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK_BUTTONS_TICKMARKS, _)).Times(0);
  scrollbar->set_needs_repaint_thumb(true);
  scrollbar->set_needs_repaint_track(false);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());

  // Enable the track.
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(0);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK_BUTTONS_TICKMARKS, _)).Times(1);
  scrollbar->set_needs_repaint_thumb(false);
  scrollbar->set_needs_repaint_track(true);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar.get());
}

}  // namespace
}  // namespace cc
