// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "base/time/time.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

class MockBrowserControlsOffsetManagerClient
    : public BrowserControlsOffsetManagerClient {
 public:
  MockBrowserControlsOffsetManagerClient(float top_controls_height,
                                         float browser_controls_show_threshold,
                                         float browser_controls_hide_threshold)
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        redraw_needed_(false),
        update_draw_properties_needed_(false),
        browser_controls_params_(
            {top_controls_height, 0, 0, 0, false, false, false}),
        bottom_controls_shown_ratio_(1.f),
        top_controls_shown_ratio_(1.f),
        browser_controls_show_threshold_(browser_controls_show_threshold),
        browser_controls_hide_threshold_(browser_controls_hide_threshold) {
    active_tree_ = std::make_unique<LayerTreeImpl>(
        host_impl_, new SyncedScale, new SyncedBrowserControls,
        new SyncedBrowserControls, new SyncedElasticOverscroll);
    root_scroll_layer_ = LayerImpl::Create(active_tree_.get(), 1);
  }

  ~MockBrowserControlsOffsetManagerClient() override = default;

  void DidChangeBrowserControlsPosition() override {
    redraw_needed_ = true;
    update_draw_properties_needed_ = true;
  }

  bool HaveRootScrollNode() const override { return true; }

  float BottomControlsHeight() const override {
    return browser_controls_params_.bottom_controls_height;
  }

  float BottomControlsMinHeight() const override {
    return browser_controls_params_.bottom_controls_min_height;
  }

  float TopControlsHeight() const override {
    return browser_controls_params_.top_controls_height;
  }

  float TopControlsMinHeight() const override {
    return browser_controls_params_.top_controls_min_height;
  }

  bool OnlyExpandTopControlsAtPageTop() const override {
    return browser_controls_params_.only_expand_top_controls_at_page_top;
  }

  gfx::PointF ViewportScrollOffset() const override {
    return viewport_scroll_offset_;
  }

  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override {
    AssertAndClamp(&top_ratio);
    top_controls_shown_ratio_ = top_ratio;

    AssertAndClamp(&bottom_ratio);
    bottom_controls_shown_ratio_ = bottom_ratio;
  }

  void AssertAndClamp(float* ratio) {
    ASSERT_FALSE(std::isnan(*ratio));
    ASSERT_FALSE(*ratio == std::numeric_limits<float>::infinity());
    ASSERT_FALSE(*ratio == -std::numeric_limits<float>::infinity());
    *ratio = std::max(*ratio, 0.f);
  }

  float CurrentBottomControlsShownRatio() const override {
    return bottom_controls_shown_ratio_;
  }

  float CurrentTopControlsShownRatio() const override {
    return top_controls_shown_ratio_;
  }

  void SetNeedsCommit() override {}

  LayerImpl* rootScrollLayer() { return root_scroll_layer_.get(); }

  BrowserControlsOffsetManager* manager() {
    if (!manager_) {
      manager_ = BrowserControlsOffsetManager::Create(
          this, browser_controls_show_threshold_,
          browser_controls_hide_threshold_);
    }
    return manager_.get();
  }

  void SetBrowserControlsParams(BrowserControlsParams params) {
    browser_controls_params_ = params;

    manager()->OnBrowserControlsParamsChanged(
        params.animate_browser_controls_height_changes);
  }

  void SetViewportScrollOffset(float x, float y) {
    viewport_scroll_offset_ = gfx::PointF(x, y);
  }

  void ScrollVerticallyBy(float dy) {
    gfx::Vector2dF viewport_scroll_delta = manager()->ScrollBy({0.f, dy});
    viewport_scroll_offset_ += viewport_scroll_delta;
  }

 private:
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  std::unique_ptr<LayerTreeImpl> active_tree_;
  std::unique_ptr<LayerImpl> root_scroll_layer_;
  std::unique_ptr<BrowserControlsOffsetManager> manager_;
  bool redraw_needed_;
  bool update_draw_properties_needed_;

  BrowserControlsParams browser_controls_params_;
  float bottom_controls_shown_ratio_;
  float top_controls_shown_ratio_;
  float browser_controls_show_threshold_;
  float browser_controls_hide_threshold_;
  gfx::PointF viewport_scroll_offset_;
};

TEST(BrowserControlsOffsetManagerTest, EnsureScrollThresholdApplied) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();

  // Scroll down to hide the controls entirely.
  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_FLOAT_EQ(-30.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_FLOAT_EQ(-60.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  // Scroll back up a bit and ensure the controls don't move until we cross
  // the threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  // After hitting the threshold, further scrolling up should result in the top
  // controls showing.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_FLOAT_EQ(-90.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_FLOAT_EQ(-40.f, manager->ControlsTopOffset());

  // Reset the scroll threshold by going further up the page than the initial
  // threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -100.f));
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());

  // See that scrolling down the page now will result in the controls hiding.
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(-20.f, manager->ControlsTopOffset());

  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     EnsureScrollThresholdAppliedWithMinHeight) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // First, set the min-height.
  client.SetBrowserControlsParams({100.f, 20.f, 0.f, 0.f, false, false});

  manager->ScrollBegin();

  // Scroll down to hide the controls.
  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_FLOAT_EQ(-30.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_FLOAT_EQ(-60.f, manager->ControlsTopOffset());

  // Controls should stop scrolling when we hit the min-height.
  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_FLOAT_EQ(-80.f, manager->ControlsTopOffset());

  // Scroll back up a bit and ensure the controls don't move until we cross
  // the threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_FLOAT_EQ(-80.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, -60.f));
  EXPECT_FLOAT_EQ(-80.f, manager->ControlsTopOffset());

  // After hitting the threshold, further scrolling up should result in the top
  // controls starting to move.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_FLOAT_EQ(-70.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_FLOAT_EQ(-20.f, manager->ControlsTopOffset());

  // Reset the scroll threshold by going further up the page than the initial
  // threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -100.f));
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());

  // See that scrolling down the page now will result in the controls hiding.
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(-20.f, manager->ControlsTopOffset());

  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, PartialShownHideAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_FLOAT_EQ(-85.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     BottomControlsPartialShownHideAnimation) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_FLOAT_EQ(0.2f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->BottomControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->BottomControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, PartialShownShowAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -70.f));
  EXPECT_FLOAT_EQ(-30.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(70.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     BottomControlsPartialShownShowAnimation) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->BottomControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->BottomControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialHiddenWithAmbiguousThresholdShows) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(-20.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(80.f, manager->ContentTopOffset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialHiddenWithAmbiguousThresholdHides) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_FLOAT_EQ(-30.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(70.f, manager->ContentTopOffset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialShownWithAmbiguousThresholdHides) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBy(gfx::Vector2dF(0.f, 200.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_FLOAT_EQ(-80.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialShownWithAmbiguousThresholdShows) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBy(gfx::Vector2dF(0.f, 200.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, -30.f));
  EXPECT_FLOAT_EQ(-70.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(30.f, manager->ContentTopOffset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, PinchIgnoresScroll) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Hide the controls.
  manager->ScrollBegin();
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  manager->PinchBegin();
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  // Scrolls are ignored during pinch.
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  manager->PinchEnd();
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  // Scrolls should no long be ignored.
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_FLOAT_EQ(-85.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->HasAnimation());
}

TEST(BrowserControlsOffsetManagerTest, PinchBeginStartsAnimationIfNecessary) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  manager->PinchBegin();
  EXPECT_FALSE(manager->HasAnimation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->HasAnimation());

  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_FLOAT_EQ(-85.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.f, manager->ContentTopOffset());

  manager->PinchBegin();
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->HasAnimation());

  manager->ScrollBy(gfx::Vector2dF(0.f, -55.f));
  EXPECT_FLOAT_EQ(-45.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(55.f, manager->ContentTopOffset());
  EXPECT_FALSE(manager->HasAnimation());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->HasAnimation());

  time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, HeightIncreaseWhenFullyShownAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the new height with animation.
  client.SetBrowserControlsParams({150, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(150.f, manager->TopControlsHeight());
  // Ratio should've been updated to avoid jumping to the new height.
  EXPECT_FLOAT_EQ(100.f / 150.f, manager->TopControlsShownRatio());
  // Min-height offset should stay 0 since only the height changed.
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeightOffset());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  // Controls should be fully shown when the animation ends.
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(150.f, manager->ContentTopOffset());
  // Min-height offset should still be 0.
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest, HeightDecreaseWhenFullyShownAnimation) {
  MockBrowserControlsOffsetManagerClient client(150.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the new height with animation.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.f, manager->TopControlsHeight());
  // Ratio should've been updated to avoid jumping to the new height.
  // The ratio will be > 1 here.
  EXPECT_FLOAT_EQ(150.f / 100.f, manager->TopControlsShownRatio());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  // Controls should be fully shown when the animation ends.
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, MinHeightIncreaseWhenHiddenAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Scroll to hide.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Set the new min-height with animation.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeight());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeightOffset());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animation.
  float previous_ratio = manager->TopControlsShownRatio();
  float previous_min_height_offset = manager->TopControlsMinHeightOffset();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous_ratio);
  EXPECT_EQ(manager->TopControlsMinHeightOffset(), previous_min_height_offset);

  while (manager->HasAnimation()) {
    previous_ratio = manager->TopControlsShownRatio();
    previous_min_height_offset = manager->TopControlsMinHeightOffset();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous_ratio);
    // Min-height offset is also animated.
    EXPECT_GT(manager->TopControlsMinHeightOffset(),
              previous_min_height_offset);
  }
  EXPECT_FALSE(manager->HasAnimation());
  // Controls should be at the new min-height when the animation ends.
  EXPECT_FLOAT_EQ(20.f / 100.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
  // Min-height offset should be equal to the min-height at the end.
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     MinHeightSetToZeroWhenAtMinHeightAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the min-height.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});

  // Scroll to min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 80.f));
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Set the new min-height with animation.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeight());
  // The controls should still be at the min-height.
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
  // Min-height offset is equal to min-height.
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeightOffset());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous_ratio = manager->TopControlsShownRatio();
  float previous_min_height_offset = manager->TopControlsMinHeightOffset();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous_ratio);

  while (manager->HasAnimation()) {
    previous_ratio = manager->TopControlsShownRatio();
    previous_min_height_offset = manager->TopControlsMinHeightOffset();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous_ratio);
    // Min-height offset is also animated.
    EXPECT_LT(manager->TopControlsMinHeightOffset(),
              previous_min_height_offset);
  }
  EXPECT_FALSE(manager->HasAnimation());
  // Controls should be hidden when the animation ends.
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  // Min-height offset will be equal to the new min-height.
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest, EnsureNoAnimationCases) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // No animation should run if only the min-height changes when the controls
  // are fully shown.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});
  EXPECT_FALSE(manager->HasAnimation());

  // Scroll to min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 80.f));
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // No animation should run if only the height changes when the controls
  // are at min-height.
  client.SetBrowserControlsParams({150, 20, 0, 0, true, false});
  EXPECT_FALSE(manager->HasAnimation());

  // Set the min-height to 0 without animation.
  client.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());

  // No animation should run if only the height changes when the controls are
  // fully hidden.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_FALSE(manager->HasAnimation());
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeAnimationJumpsToEndOnScroll) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(100.f, manager->ContentTopOffset());

  // Change the params to start an animation.
  client.SetBrowserControlsParams({150, 30, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();
  // First animate will establish the animation.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  // Forward a little bit.
  time = base::Microseconds(100) + time;
  manager->Animate(time);

  // Animation should be in progress.
  EXPECT_GT(manager->TopControlsShownRatio(), previous);

  manager->ScrollBegin();
  // Scroll should cause the animation to jump to the end.
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(150.f, manager->ContentTopOffset());
  // Min-height offset should jump to the new min-height.
  EXPECT_FLOAT_EQ(30.f, manager->TopControlsMinHeightOffset());
  EXPECT_FALSE(manager->HasAnimation());
  // Then, the scroll will move the controls as it would normally.
  manager->ScrollBy(gfx::Vector2dF(0.f, 60.f));
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(90.f, manager->ContentTopOffset());
  // Min-height offset won't change once the animation is complete.
  EXPECT_FLOAT_EQ(30.f, manager->TopControlsMinHeightOffset());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeMaintainsFullyVisibleControls) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());

  client.SetBrowserControlsParams({100, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0, manager->ControlsTopOffset());

  client.SetBrowserControlsParams({50, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(50.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     ShrinkingHeightKeepsBrowserControlsHidden) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  client.SetBrowserControlsParams({50, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(-50.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());

  client.SetBrowserControlsParams({0, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeWithAnimateFalseDoesNotTriggerAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  client.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(150.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0, manager->ControlsTopOffset());

  client.SetBrowserControlsParams({50, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(50.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     MinHeightChangeWithAnimateFalseSnapsToNewMinHeight) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());

  // Scroll to hide the controls.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Change the min-height from 0 to 20.
  client.SetBrowserControlsParams({100, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeight());
  // Top controls should snap to the new min-height.
  EXPECT_FLOAT_EQ(-80.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
  // Min-height offset snaps to the new min-height.
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeightOffset());

  // Change the min-height from 20 to 0.
  client.SetBrowserControlsParams({100, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeight());
  // Top controls should snap to the new min-height, 0.
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  // Min-height offset snaps to the new min-height.
  EXPECT_FLOAT_EQ(0.f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest, ControlsStayAtMinHeightOnHeightChange) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the min-height to 20.
  client.SetBrowserControlsParams({100, 20, 0, 0, false, false});

  // Scroll the controls to min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Change the height from 100 to 120.
  client.SetBrowserControlsParams({120, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(120.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeight());
  // Top controls should stay at the same visible height.
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());

  // Change the height from 120 back to 100.
  client.SetBrowserControlsParams({100, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeight());
  // Top controls should still stay at the same visible height.
  EXPECT_FLOAT_EQ(-80.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, ControlsAdjustToNewHeight) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Scroll the controls a little.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 40.f));
  EXPECT_FLOAT_EQ(60.f, manager->ContentTopOffset());

  // Change the height from 100 to 120.
  client.SetBrowserControlsParams({120, 0, 0, 0, false, false});

  // The shown ratios should be adjusted to keep the shown height same.
  EXPECT_FLOAT_EQ(60.f, manager->ContentTopOffset());

  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollByWithZeroHeightControlsIsNoop) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kBoth, false,
                                      std::nullopt);

  manager->ScrollBegin();
  gfx::Vector2dF pending = manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(20.f, pending.y());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollThenRestoreBottomControls) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(80.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -200.f));
  EXPECT_FLOAT_EQ(100.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     ScrollThenRestoreBottomControlsNoTopControls) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(80.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -200.f));
  EXPECT_FLOAT_EQ(100.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, HideAndPeekBottomControls) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(0.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_FLOAT_EQ(15.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.15f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     HideAndImmediateShowKeepsControlsVisible) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kHidden, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown, true,
                                      std::nullopt);
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest,
     ScrollWithMinHeightSetForTopControlsOnly) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 30, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  // Controls don't hide completely and stop at the min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, 150));
  EXPECT_FLOAT_EQ(30.f / 100, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();

  // Controls scroll immediately from the min-height point.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, -70));
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollWithMinHeightSetForBothControls) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 30, 100, 20, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  // Controls don't hide completely and stop at the min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, 150));
  EXPECT_FLOAT_EQ(30.f / 100, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(20.f / 100, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, -70));
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ChangingBottomHeightFromZeroAnimates) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 30, 0, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  // Set the bottom controls height to 100 with animation.
  client.SetBrowserControlsParams({100, 30, 100, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  // The bottom controls should be hidden in the beginning.
  EXPECT_FLOAT_EQ(0.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.f, client.CurrentBottomControlsShownRatio());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous_ratio = manager->BottomControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->BottomControlsShownRatio(), previous_ratio);

  while (manager->HasAnimation()) {
    previous_ratio = manager->BottomControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->BottomControlsShownRatio(), previous_ratio);
  }

  // Now the bottom controls should be fully shown.
  EXPECT_FLOAT_EQ(100.f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest,
     ChangingControlsHeightToZeroWithAnimationIsNoop) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 20, 80, 10, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  // Set the bottom controls height to 0 with animation.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});

  // There shouldn't be an animation because we can't animate controls with 0
  // height.
  EXPECT_FALSE(manager->HasAnimation());
  // Also, the bottom controls ratio should stay the same.
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());

  // Increase the top controls height with animation.
  client.SetBrowserControlsParams({120, 20, 0, 0, true, false});
  // This shouldn't override the bottom controls shown ratio.
  EXPECT_FLOAT_EQ(1.f, client.CurrentBottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest, OnlyExpandTopControlsAtPageTop) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  client.SetBrowserControlsParams(
      {/*top_controls_height=*/100.f, 0, 0, 0, false, false,
       /*only_expand_top_controls_at_page_top=*/true});
  BrowserControlsOffsetManager* manager = client.manager();

  // Scroll down to hide the controls entirely.
  manager->ScrollBegin();
  client.ScrollVerticallyBy(150.f);
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(50.f, client.ViewportScrollOffset().y());
  manager->ScrollEnd();

  manager->ScrollBegin();

  // Scroll back up a bit and ensure the controls don't move until we're at
  // the top.
  client.ScrollVerticallyBy(-20.f);
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(30.f, client.ViewportScrollOffset().y());

  client.ScrollVerticallyBy(-10.f);
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.f, client.ViewportScrollOffset().y());

  // After scrolling past the top, the top controls should start showing.
  client.ScrollVerticallyBy(-30.f);
  EXPECT_FLOAT_EQ(-90.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, client.ViewportScrollOffset().y());

  client.ScrollVerticallyBy(-50.f);
  EXPECT_FLOAT_EQ(-40.f, manager->ControlsTopOffset());
  // The final offset is greater than gtest's epsilon.
  EXPECT_GT(0.0001f, client.ViewportScrollOffset().y());

  manager->ScrollEnd();
}

// Tests that if the min-height changes while we're animating to the previous
// min-height, the animation gets updated to end at the new value.
TEST(BrowserControlsOffsetManagerTest, MinHeightChangeUpdatesAnimation) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams(
      {/*top_controls_height=*/100, /*top_controls_min_height=*/50, 0, 0,
       /*animate_browser_controls_height_changes=*/true, false, false});
  BrowserControlsOffsetManager* manager = client.manager();

  // Hide the controls to start an animation to min-height.
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  manager->UpdateBrowserControlsState(BrowserControlsState::kHidden,
                                      BrowserControlsState::kBoth, true,
                                      std::nullopt);
  base::TimeTicks time = base::TimeTicks::Now();
  manager->Animate(time);
  EXPECT_TRUE(manager->HasAnimation());

  // Update the min-height to a smaller value.
  client.SetBrowserControlsParams(
      {/*top_controls_height=*/100, /*top_controls_min_height=*/10, 0, 0,
       /*animate_browser_controls_height_changes=*/true, false, false});
  EXPECT_TRUE(manager->HasAnimation());

  // Make sure the animation finishes at the new min-height.
  while (manager->HasAnimation()) {
    time = base::Microseconds(100) + time;
    manager->Animate(time);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.1f, manager->TopControlsShownRatio());
}

// Tests that setting a top height and min-height with animation when both were
// 0 doesn't cause invalid |TopControlsMinHeightOffset| values.
// See: https://crbug.com/1184902.
TEST(BrowserControlsOffsetManagerTest,
     ChangingTopMinHeightFromInitialZeroAnimatesCorrectly) {
  MockBrowserControlsOffsetManagerClient client(0, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  client.SetBrowserControlsParams({100, 30, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.f, client.CurrentTopControlsShownRatio());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animation.
  float previous_min_height_offset = 0.f;
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsMinHeightOffset(), previous_min_height_offset);

  while (manager->HasAnimation()) {
    previous_min_height_offset = manager->TopControlsMinHeightOffset();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GE(manager->TopControlsMinHeightOffset(),
              previous_min_height_offset);
    EXPECT_LE(manager->TopControlsMinHeightOffset(),
              manager->TopControlsMinHeight());
  }

  EXPECT_FLOAT_EQ(30.f, manager->TopControlsMinHeightOffset());
}

// Tests that reducing both height and min-height with animation doesn't cause
// invalid |TopControlsMinHeightOffset| values.
TEST(BrowserControlsOffsetManagerTest,
     ReducingTopHeightAndMinHeightAnimatesCorrectly) {
  MockBrowserControlsOffsetManagerClient client(0, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  client.SetBrowserControlsParams({100, 30, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_EQ(30, manager->TopControlsMinHeightOffset());
  client.SetBrowserControlsParams({50, 20, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animation.
  float previous_min_height_offset = 30.f;
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsMinHeightOffset(), previous_min_height_offset);

  while (manager->HasAnimation()) {
    previous_min_height_offset = manager->TopControlsMinHeightOffset();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LE(manager->TopControlsMinHeightOffset(),
              previous_min_height_offset);
    EXPECT_GE(manager->TopControlsMinHeightOffset(),
              manager->TopControlsMinHeight());
  }

  EXPECT_FLOAT_EQ(20.f, manager->TopControlsMinHeightOffset());
}

// Tests that a "show" animation that's interrupted by a scroll is restarted
// when the gesture completes.
TEST(BrowserControlsOffsetManagerTest,
     InterruptedShowAnimationsAreRestartedAfterScroll) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  // Start off with the controls mostly hidden, so that they will, by default,
  // try to fully hide at the end of a scroll.
  client.SetCurrentBrowserControlsShownRatio(/*top_ratio=*/0.2,
                                             /*bottom_ratio=*/0.2);

  EXPECT_FALSE(manager->HasAnimation());

  // Start an animation to show the controls
  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->IsAnimatingToShowControls());

  // Start a scroll, which should cancel the animation.
  manager->ScrollBegin();
  EXPECT_FALSE(manager->HasAnimation());

  // Finish the scroll, which should restart the show animation.  Since the
  // animation didn't run yet, the controls would auto-hide otherwise since they
  // started almost hidden.
  manager->ScrollEnd();
  EXPECT_TRUE(manager->IsAnimatingToShowControls());
}

// If chrome tries to animate in browser controls during a scroll gesture, it
// should animate them in after the scroll completes.
TEST(BrowserControlsOffsetManagerTest,
     ShowingControlsDuringScrollStartsAnimationAfterScroll) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  // Start off with the controls mostly hidden, so that they will, by default,
  // try to fully hide at the end of a scroll.
  client.SetCurrentBrowserControlsShownRatio(/*top_ratio=*/0.2,
                                             /*bottom_ratio=*/0.2);

  EXPECT_FALSE(manager->HasAnimation());

  // Start a scroll. Make sure that there's no animation running, else we're
  // testing the wrong case.
  ASSERT_FALSE(manager->HasAnimation());
  manager->ScrollBegin();
  EXPECT_FALSE(manager->HasAnimation());

  // Start an animation to show the controls.
  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->IsAnimatingToShowControls());

  // Finish the scroll, and the animation should still be in progress and/or
  // restarted.  We don't really care which, as long as it wasn't cancelled and
  // is trying to show the controls.
  manager->ScrollEnd();
  EXPECT_TRUE(manager->IsAnimatingToShowControls());
}

TEST(BrowserControlsOffsetManagerTest, MinHeightIncreasedByMoreThanHeight) {
  MockBrowserControlsOffsetManagerClient client(150.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set a starting height without animation.
  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 50,
       /* bottom_controls_min_height */ 0,
       /* animate_browser_controls_height_changes */ false,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(50.f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsMinHeight());
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsMinHeightOffset());
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
  EXPECT_FALSE(manager->HasAnimation());

  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 200,
       /* bottom_controls_min_height */ 200,
       /* animate_browser_controls_height_changes */ true,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(200.f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(200.f, manager->BottomControlsMinHeight());
  // The min height offset should have been "stepped up" to match the previous
  // height of 50, so that it animates over the same range of values as the
  // height does.
  EXPECT_FLOAT_EQ(50.f, manager->BottomControlsMinHeightOffset());
  // With the animation, the shown ratio won't change just yet. With a
  // transition from 50 -> 200, only 25% is currently showing.
  EXPECT_FLOAT_EQ(0.25f, manager->BottomControlsShownRatio());
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();
  // First animate will establish the animation.
  float previous = manager->BottomControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->BottomControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->BottomControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->BottomControlsShownRatio(), previous);

    // Ensure that the min height offset is moving in coordination with the
    // shown ratio.
    EXPECT_FLOAT_EQ(manager->BottomControlsMinHeightOffset() /
                        manager->BottomControlsMinHeight(),
                    manager->BottomControlsShownRatio());
  }
  EXPECT_FALSE(manager->HasAnimation());
  // Controls should be fully shown when the animation ends.
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest, MinHeightDecreasedByMoreThanHeight) {
  MockBrowserControlsOffsetManagerClient client(150.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set a starting height without animation.
  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 200,
       /* bottom_controls_min_height */ 200,
       /* animate_browser_controls_height_changes */ false,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(200.f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(200.f, manager->BottomControlsMinHeight());
  EXPECT_FLOAT_EQ(200.f, manager->BottomControlsMinHeightOffset());
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
  EXPECT_FALSE(manager->HasAnimation());

  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 50,
       /* bottom_controls_min_height */ 0,
       /* animate_browser_controls_height_changes */ true,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(50.f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsMinHeight());
  // The min height offset should still be at the full value, as the animation
  // hasn't yet started.
  EXPECT_FLOAT_EQ(200.f, manager->BottomControlsMinHeightOffset());
  // With the animation, the shown ratio won't change just yet. With a
  // transition from 200 -> 50, 400% is currently showing.
  EXPECT_FLOAT_EQ(4.0f, manager->BottomControlsShownRatio());
  EXPECT_TRUE(manager->HasAnimation());

  base::TimeTicks time = base::TimeTicks::Now();
  // First animate will establish the animation.
  float previous = manager->BottomControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->BottomControlsShownRatio(), previous);

  while (manager->HasAnimation()) {
    previous = manager->BottomControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->BottomControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->HasAnimation());
  // Controls should be fully shown when the animation ends.
  EXPECT_FLOAT_EQ(1.f, manager->BottomControlsShownRatio());
  // Ensure that the min height offset has fully shrunk.
  EXPECT_FLOAT_EQ(0.f, manager->BottomControlsMinHeightOffset());
}

}  // namespace
}  // namespace cc
