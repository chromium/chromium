// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/browser_controls_params.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

constexpr int kDeviceFramesPerSecond = 60;

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
        bottom_controls_shown_ratio_(1.0f),
        top_controls_shown_ratio_(1.0f),
        browser_controls_show_threshold_(browser_controls_show_threshold),
        browser_controls_hide_threshold_(browser_controls_hide_threshold) {
    active_tree_ = std::make_unique<LayerTreeImpl>(
        host_impl_, viz::BeginFrameArgs(), new SyncedScale,
        new SyncedBrowserControls, new SyncedBrowserControls);
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
    *ratio = std::max(*ratio, 0.0f);
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

  void ScrollVerticallyBy(float dy, bool is_inertial = false) {
    gfx::Vector2dF viewport_scroll_delta =
        manager()->ScrollBy({0.0f, dy}, is_inertial);
    viewport_scroll_offset_ += viewport_scroll_delta;
  }

  base::TimeDelta CurrentFrameInterval() const override {
    return base::Seconds(1) / kDeviceFramesPerSecond;
  }

  float RenderedDeviceScaleFactor() const override { return 1.0f; }

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
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();

  // Scroll down to hide the controls entirely.
  manager->ScrollBy(gfx::Vector2dF(0.0f, 30.0f));
  EXPECT_FLOAT_EQ(-30.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, 30.0f));
  EXPECT_FLOAT_EQ(-60.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, 100.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  // Scroll back up a bit and ensure the controls don't move until we cross
  // the threshold.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -10.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, -50.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  // After hitting the threshold, further scrolling up should result in the top
  // controls showing.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -10.0f));
  EXPECT_FLOAT_EQ(-90.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, -50.0f));
  EXPECT_FLOAT_EQ(-40.0f, manager->ControlsTopOffset());

  // Reset the scroll threshold by going further up the page than the initial
  // threshold.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -100.0f));
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // See that scrolling down the page now will result in the controls hiding.
  manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(-20.0f, manager->ControlsTopOffset());

  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     EnsureScrollThresholdAppliedWithMinHeight) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // First, set the min-height.
  client.SetBrowserControlsParams({100.0f, 20.0f, 0.0f, 0.0f, false, false});

  manager->ScrollBegin();

  // Scroll down to hide the controls.
  manager->ScrollBy(gfx::Vector2dF(0.0f, 30.0f));
  EXPECT_FLOAT_EQ(-30.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, 30.0f));
  EXPECT_FLOAT_EQ(-60.0f, manager->ControlsTopOffset());

  // Controls should stop scrolling when we hit the min-height.
  manager->ScrollBy(gfx::Vector2dF(0.0f, 100.0f));
  EXPECT_FLOAT_EQ(-80.0f, manager->ControlsTopOffset());

  // Scroll back up a bit and ensure the controls don't move until we cross
  // the threshold.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -20.0f));
  EXPECT_FLOAT_EQ(-80.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, -60.0f));
  EXPECT_FLOAT_EQ(-80.0f, manager->ControlsTopOffset());

  // After hitting the threshold, further scrolling up should result in the top
  // controls starting to move.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -10.0f));
  EXPECT_FLOAT_EQ(-70.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, -50.0f));
  EXPECT_FLOAT_EQ(-20.0f, manager->ControlsTopOffset());

  // Reset the scroll threshold by going further up the page than the initial
  // threshold.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -100.0f));
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // See that scrolling down the page now will result in the controls hiding.
  manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(-20.0f, manager->ControlsTopOffset());

  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, PartialShownHideAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, -15.0f));
  EXPECT_FLOAT_EQ(-85.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.0f, manager->ContentTopOffset());
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
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     BottomControlsPartialShownHideAnimation) {
  MockBrowserControlsOffsetManagerClient client(0.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, -20.0f));
  EXPECT_FLOAT_EQ(0.2f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
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
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, PartialShownShowAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, -70.0f));
  EXPECT_FLOAT_EQ(-30.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(70.0f, manager->ContentTopOffset());
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
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     BottomControlsPartialShownShowAnimation) {
  MockBrowserControlsOffsetManagerClient client(0.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
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
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialHiddenWithAmbiguousThresholdShows) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(-20.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(80.0f, manager->ContentTopOffset());

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
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialHiddenWithAmbiguousThresholdHides) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.0f, 30.0f));
  EXPECT_FLOAT_EQ(-30.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(70.0f, manager->ContentTopOffset());

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
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialShownWithAmbiguousThresholdHides) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBy(gfx::Vector2dF(0.0f, 200.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.0f, -20.0f));
  EXPECT_FLOAT_EQ(-80.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());

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
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     PartialShownWithAmbiguousThresholdShows) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.25f, 0.25f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBy(gfx::Vector2dF(0.0f, 200.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.0f, -30.0f));
  EXPECT_FLOAT_EQ(-70.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(30.0f, manager->ContentTopOffset());

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
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, PinchIgnoresScroll) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Hide the controls.
  manager->ScrollBegin();
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  manager->PinchBegin();
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  // Scrolls are ignored during pinch.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -15.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  manager->PinchEnd();
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  // Scrolls should no long be ignored.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -15.0f));
  EXPECT_FLOAT_EQ(-85.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->HasAnimation());
}

TEST(BrowserControlsOffsetManagerTest, PinchBeginStartsAnimationIfNecessary) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());

  manager->PinchBegin();
  EXPECT_FALSE(manager->HasAnimation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->HasAnimation());

  manager->ScrollBy(gfx::Vector2dF(0.0f, -15.0f));
  EXPECT_FLOAT_EQ(-85.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.0f, manager->ContentTopOffset());

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

  manager->ScrollBy(gfx::Vector2dF(0.0f, -55.0f));
  EXPECT_FLOAT_EQ(-45.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(55.0f, manager->ContentTopOffset());
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
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, HeightIncreaseWhenFullyShownAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the new height with animation.
  client.SetBrowserControlsParams({150, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(150.0f, manager->TopControlsHeight());
  // Ratio should've been updated to avoid jumping to the new height.
  EXPECT_FLOAT_EQ(100.0f / 150.0f, manager->TopControlsShownRatio());
  // Min-height offset should stay 0 since only the height changed.
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeightOffset());

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
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(150.0f, manager->ContentTopOffset());
  // Min-height offset should still be 0.
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest, HeightDecreaseWhenFullyShownAnimation) {
  MockBrowserControlsOffsetManagerClient client(150.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the new height with animation.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.0f, manager->TopControlsHeight());
  // Ratio should've been updated to avoid jumping to the new height.
  // The ratio will be > 1 here.
  EXPECT_FLOAT_EQ(150.0f / 100.0f, manager->TopControlsShownRatio());

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
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, MinHeightIncreaseWhenHiddenAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Scroll to hide.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 100.0f));
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Set the new min-height with animation.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeightOffset());

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
  EXPECT_FLOAT_EQ(20.0f / 100.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
  // Min-height offset should be equal to the min-height at the end.
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     MinHeightSetToZeroWhenAtMinHeightAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the min-height.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});

  // Scroll to min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 80.0f));
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Set the new min-height with animation.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeight());
  // The controls should still be at the min-height.
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
  // Min-height offset is equal to min-height.
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeightOffset());

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
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  // Min-height offset will be equal to the new min-height.
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest, EnsureNoAnimationCases) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // No animation should run if only the min-height changes when the controls
  // are fully shown.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});
  EXPECT_FALSE(manager->HasAnimation());

  // Scroll to min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 80.0f));
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // No animation should run if only the height changes when the controls
  // are at min-height.
  client.SetBrowserControlsParams({150, 20, 0, 0, true, false});
  EXPECT_FALSE(manager->HasAnimation());

  // Set the min-height to 0 without animation.
  client.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());

  // No animation should run if only the height changes when the controls are
  // fully hidden.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_FALSE(manager->HasAnimation());
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeAnimationJumpsToEndOnScroll) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(100.0f, manager->ContentTopOffset());

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
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(150.0f, manager->ContentTopOffset());
  // Min-height offset should jump to the new min-height.
  EXPECT_FLOAT_EQ(30.0f, manager->TopControlsMinHeightOffset());
  EXPECT_FALSE(manager->HasAnimation());
  // Then, the scroll will move the controls as it would normally.
  manager->ScrollBy(gfx::Vector2dF(0.0f, 60.0f));
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(90.0f, manager->ContentTopOffset());
  // Min-height offset won't change once the animation is complete.
  EXPECT_FLOAT_EQ(30.0f, manager->TopControlsMinHeightOffset());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeMaintainsFullyVisibleControls) {
  MockBrowserControlsOffsetManagerClient client(0.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  client.SetBrowserControlsParams({100, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0, manager->ControlsTopOffset());

  client.SetBrowserControlsParams({50, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(50.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     ShrinkingHeightKeepsBrowserControlsHidden) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  client.SetBrowserControlsParams({50, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(-50.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());

  client.SetBrowserControlsParams({0, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeWithAnimateFalseDoesNotTriggerAnimation) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  client.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(150.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0, manager->ControlsTopOffset());

  client.SetBrowserControlsParams({50, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(50.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     MinHeightChangeWithAnimateFalseSnapsToNewMinHeight) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // Scroll to hide the controls.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 100.0f));
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Change the min-height from 0 to 20.
  client.SetBrowserControlsParams({100, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeight());
  // Top controls should snap to the new min-height.
  EXPECT_FLOAT_EQ(-80.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
  // Min-height offset snaps to the new min-height.
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeightOffset());

  // Change the min-height from 20 to 0.
  client.SetBrowserControlsParams({100, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeight());
  // Top controls should snap to the new min-height, 0.
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  // Min-height offset snaps to the new min-height.
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     MinHeightChangeInHiddenStateSnapsToNewMinHeight) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set min_height == height, so controls are fully visible but at min height.
  client.SetBrowserControlsParams({100, 100, 0, 0, false, false});
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // Set the state to hidden. Since min_height is 100, this does nothing.
  manager->UpdateBrowserControlsState(BrowserControlsState::kHidden,
                                      BrowserControlsState::kBoth, false,
                                      std::nullopt);
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // Now, change the min_height to 0. Because the state is kHidden, the
  // controls should snap to their new minimum shown ratio (0).
  client.SetBrowserControlsParams({100, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     MinHeightChangeInHiddenStateAnimatesToNewMinHeight) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set min_height == height, so controls are fully visible but at min height.
  client.SetBrowserControlsParams({100, 100, 0, 0, false, false});
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // Set the state to hidden. Since min_height is 100, this does nothing.
  manager->UpdateBrowserControlsState(BrowserControlsState::kHidden,
                                      BrowserControlsState::kBoth, false,
                                      std::nullopt);
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());

  // Now, change the min_height to 0 with animation.
  client.SetBrowserControlsParams({100, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeight());
  // The ratio should not have changed yet.
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());

  base::TimeTicks time = base::TimeTicks::Now();
  // First animate will establish the animation.
  float previous_ratio = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous_ratio);

  while (manager->HasAnimation()) {
    previous_ratio = manager->TopControlsShownRatio();
    time = base::Microseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous_ratio);
  }

  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, ControlsStayAtMinHeightOnHeightChange) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the min-height to 20.
  client.SetBrowserControlsParams({100, 20, 0, 0, false, false});

  // Scroll the controls to min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 100.0f));
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
  manager->ScrollEnd();

  // Change the height from 100 to 120.
  client.SetBrowserControlsParams({120, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(120.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeight());
  // Top controls should stay at the same visible height.
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());

  // Change the height from 120 back to 100.
  client.SetBrowserControlsParams({100, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeight());
  // Top controls should still stay at the same visible height.
  EXPECT_FLOAT_EQ(-80.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.0f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     ControlsStayAtFullHeightWhenPreviousMinHeightEqualledFullHeight) {
  MockBrowserControlsOffsetManagerClient client(20.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set the min-height to 20.
  client.SetBrowserControlsParams({20, 20, 0, 0, false, false});

  // Change the height to 120.
  client.SetBrowserControlsParams({120, 20, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(120.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeight());
  // Top controls should stay at the same visible height.
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(120.0f, manager->ContentTopOffset());

  // Repeat the above for bottom controls.
  client.SetBrowserControlsParams({0, 0, 20, 20, false, false});
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});

  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(100.0f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsMinHeight());
  // Bottom controls should stay at the same visible height.
  EXPECT_FLOAT_EQ(100.0f, manager->ContentBottomOffset());
}

TEST(BrowserControlsOffsetManagerTest, ControlsAdjustToNewHeight) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Scroll the controls a little.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 40.0f));
  EXPECT_FLOAT_EQ(60.0f, manager->ContentTopOffset());

  // Change the height from 100 to 120.
  client.SetBrowserControlsParams({120, 0, 0, 0, false, false});

  // The shown ratios should be adjusted to keep the shown height same.
  EXPECT_FLOAT_EQ(60.0f, manager->ContentTopOffset());

  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollByWithZeroHeightControlsIsNoop) {
  MockBrowserControlsOffsetManagerClient client(0.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kBoth, false,
                                      std::nullopt);

  manager->ScrollBegin();
  gfx::Vector2dF pending = manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(20.0f, pending.y());
  EXPECT_FLOAT_EQ(0.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->ContentTopOffset());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollThenRestoreBottomControls) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(80.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, -200.0f));
  EXPECT_FLOAT_EQ(100.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     ScrollThenRestoreBottomControlsNoTopControls) {
  MockBrowserControlsOffsetManagerClient client(0.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 20.0f));
  EXPECT_FLOAT_EQ(80.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, -200.0f));
  EXPECT_FLOAT_EQ(100.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, HideAndPeekBottomControls) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 300.0f));
  EXPECT_FLOAT_EQ(0.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, -15.0f));
  EXPECT_FLOAT_EQ(15.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.15f, manager->BottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest,
     HideAndImmediateShowKeepsControlsVisible) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams({0, 0, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kHidden, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown, true,
                                      std::nullopt);
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest,
     ScrollWithMinHeightSetForTopControlsOnly) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 30, 100, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  // Controls don't hide completely and stop at the min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, 150));
  EXPECT_FLOAT_EQ(30.0f / 100, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();

  // Controls scroll immediately from the min-height point.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, -70));
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollWithMinHeightSetForBothControls) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 30, 100, 20, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  // Controls don't hide completely and stop at the min-height.
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, 150));
  EXPECT_FLOAT_EQ(30.0f / 100, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(20.0f / 100, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0, -70));
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ChangingBottomHeightFromZeroAnimates) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 30, 0, 0, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  // Set the bottom controls height to 100 with animation.
  client.SetBrowserControlsParams({100, 30, 100, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());
  // The bottom controls should be hidden in the beginning.
  EXPECT_FLOAT_EQ(0.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(0.0f, client.CurrentBottomControlsShownRatio());

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
  EXPECT_FLOAT_EQ(100.0f, manager->ContentBottomOffset());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest,
     ChangingControlsHeightToZeroWithAnimationIsNoop) {
  MockBrowserControlsOffsetManagerClient client(100, 0.5f, 0.5f);
  client.SetBrowserControlsParams({100, 20, 80, 10, false, false});
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.0f, client.CurrentTopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  // Set the bottom controls height to 0 with animation.
  client.SetBrowserControlsParams({100, 20, 0, 0, true, false});

  // There shouldn't be an animation because we can't animate controls with 0
  // height.
  EXPECT_FALSE(manager->HasAnimation());
  // Also, the bottom controls ratio should stay the same.
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());

  // Increase the top controls height with animation.
  client.SetBrowserControlsParams({120, 20, 0, 0, true, false});
  // This shouldn't override the bottom controls shown ratio.
  EXPECT_FLOAT_EQ(1.0f, client.CurrentBottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest, OnlyExpandTopControlsAtPageTop) {
  MockBrowserControlsOffsetManagerClient client(0.0f, 0.5f, 0.5f);
  client.SetBrowserControlsParams(
      {/*top_controls_height=*/100.0f, 0, 0, 0, false, false,
       /*only_expand_top_controls_at_page_top=*/true});
  BrowserControlsOffsetManager* manager = client.manager();

  // Scroll down to hide the controls entirely.
  manager->ScrollBegin();
  client.ScrollVerticallyBy(150.0f);
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(50.0f, client.ViewportScrollOffset().y());
  manager->ScrollEnd();

  manager->ScrollBegin();

  // Scroll back up a bit and ensure the controls don't move until we're at
  // the top.
  client.ScrollVerticallyBy(-20.0f);
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(30.0f, client.ViewportScrollOffset().y());

  client.ScrollVerticallyBy(-10.0f);
  EXPECT_FLOAT_EQ(-100.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(20.0f, client.ViewportScrollOffset().y());

  // After scrolling past the top, the top controls should start showing.
  client.ScrollVerticallyBy(-30.0f);
  EXPECT_FLOAT_EQ(-90.0f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.0f, client.ViewportScrollOffset().y());

  client.ScrollVerticallyBy(-50.0f);
  EXPECT_FLOAT_EQ(-40.0f, manager->ControlsTopOffset());
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
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
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
  EXPECT_FLOAT_EQ(0.0f, client.CurrentTopControlsShownRatio());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animation.
  float previous_min_height_offset = 0.0f;
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

  EXPECT_FLOAT_EQ(30.0f, manager->TopControlsMinHeightOffset());
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
  float previous_min_height_offset = 30.0f;
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

  EXPECT_FLOAT_EQ(20.0f, manager->TopControlsMinHeightOffset());
}

// Tests that a "show" animation that's interrupted by a scroll is restarted
// when the gesture completes.
TEST(BrowserControlsOffsetManagerTest,
     InterruptedShowAnimationsAreRestartedAfterScroll) {
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
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
  MockBrowserControlsOffsetManagerClient client(100.0f, 0.5f, 0.5f);
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
  MockBrowserControlsOffsetManagerClient client(150.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set a starting height without animation.
  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 50,
       /* bottom_controls_min_height */ 0,
       /* animate_browser_controls_height_changes */ false,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(50.0f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsMinHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsMinHeightOffset());
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
  EXPECT_FALSE(manager->HasAnimation());

  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 200,
       /* bottom_controls_min_height */ 200,
       /* animate_browser_controls_height_changes */ true,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(200.0f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(200.0f, manager->BottomControlsMinHeight());
  // The min height offset should have been "stepped up" to match the previous
  // height of 50, so that it animates over the same range of values as the
  // height does.
  EXPECT_FLOAT_EQ(50.0f, manager->BottomControlsMinHeightOffset());
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
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
}

TEST(BrowserControlsOffsetManagerTest, MinHeightDecreasedByMoreThanHeight) {
  MockBrowserControlsOffsetManagerClient client(150.0f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  // Set a starting height without animation.
  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 200,
       /* bottom_controls_min_height */ 200,
       /* animate_browser_controls_height_changes */ false,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(200.0f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(200.0f, manager->BottomControlsMinHeight());
  EXPECT_FLOAT_EQ(200.0f, manager->BottomControlsMinHeightOffset());
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
  EXPECT_FALSE(manager->HasAnimation());

  client.SetBrowserControlsParams(
      {/* top_controls_height */ 80,
       /* top_controls_min_height */ 0,
       /* bottom_controls_height */ 50,
       /* bottom_controls_min_height */ 0,
       /* animate_browser_controls_height_changes */ true,
       /* browser_controls_shrink_blink_size */ false});
  EXPECT_FLOAT_EQ(80.0f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());
  EXPECT_FLOAT_EQ(50.0f, manager->BottomControlsHeight());
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsMinHeight());
  // The min height offset should still be at the full value, as the animation
  // hasn't yet started.
  EXPECT_FLOAT_EQ(200.0f, manager->BottomControlsMinHeightOffset());
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
  EXPECT_FLOAT_EQ(1.0f, manager->BottomControlsShownRatio());
  // Ensure that the min height offset has fully shrunk.
  EXPECT_FLOAT_EQ(0.0f, manager->BottomControlsMinHeightOffset());
}

TEST(BrowserControlsOffsetManagerTest, ShowAnimateToleratesTopAlreadyShown) {
  MockBrowserControlsOffsetManagerClient client(
      /*top_controls_height=*/100.0f,
      /*browser_controls_show_threshold=*/0.5f,
      /*float browser_controls_hide_threshold=*/0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  client.SetBrowserControlsParams(
      {/*top_controls_height=*/100, /*top_controls_min_height=*/0,
       /*bottom_controls_height=*/100, /*bottom_controls_min_height=*/0,
       /*animate_browser_controls_height_changes=*/false,
       /*browser_controls_shrink_blink_size=*/false});
  client.SetCurrentBrowserControlsShownRatio(/*top_ratio=*/1.0,
                                             /*bottom_ratio=*/0.0);
  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->HasAnimation());
  base::TimeTicks time = base::TimeTicks::Now();

  while (manager->HasAnimation()) {
    time = base::Microseconds(100) + time;
    manager->Animate(time);
  }
  EXPECT_FALSE(manager->HasAnimation());
}

TEST(BrowserControlsOffsetManagerTest,
     ScrollWithMinBottomHeightEqualToTotalBottomHeight) {
  MockBrowserControlsOffsetManagerClient client(
      /*top_controls_height=*/100.0f,
      /*browser_controls_show_threshold=*/0.5f,
      /*float browser_controls_hide_threshold=*/0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  client.SetBrowserControlsParams(
      {/*top_controls_height=*/0, /*top_controls_min_height=*/0,
       /*bottom_controls_height=*/100, /*bottom_controls_min_height=*/100,
       /*animate_browser_controls_height_changes=*/false,
       /*browser_controls_shrink_blink_size=*/false});

  // ScrollVerticallyBy will trip an assertion in
  // MockBrowserControlsOffsetManagerClient::AssertAndClamp if the resulting
  // ratio is NaN which is the bug being tested here.
  manager->ScrollBegin();
  client.ScrollVerticallyBy(30.0f);
  manager->ScrollEnd();
  EXPECT_FLOAT_EQ(30.0f, client.ViewportScrollOffset().y());
}

TEST(BrowserControlsOffsetManagerTest, SmoothScrollPreventsInstantJump) {
  constexpr float kControlsHeight = 100.0f;
  MockBrowserControlsOffsetManagerClient client(
      /*top_controls_height=*/kControlsHeight,
      /*browser_controls_show_threshold=*/0.5f,
      /*browser_controls_hide_threshold=*/0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio());

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, 2 * kControlsHeight));
  manager->ScrollEnd();

  EXPECT_FLOAT_EQ(0.0f, manager->TopControlsShownRatio());

  base::test::ScopedFeatureList feature_list(
      features::kBrowserControlsSmoothScroll);

  const int kNumAnimationFrames =
      std::ceil(1 / manager->MaximumShownRatioDeltaPerFrame(0.0f));
  manager->ScrollBegin();
  for (int i = 0; i < kNumAnimationFrames; i++) {
    manager->ScrollBy(gfx::Vector2dF(0.0f, -kControlsHeight));
    if (i < kNumAnimationFrames - 1) {
      EXPECT_LT(manager->TopControlsShownRatio(), 1.0f) << "Frame #" << i + 1;
    } else {
      EXPECT_FLOAT_EQ(1.0f, manager->TopControlsShownRatio())
          << "Frame #" << i + 1;
    }
  }
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollWithLatencyCompensation) {
  constexpr float kControlsHeight = 100.0f;
  MockBrowserControlsOffsetManagerClient client(
      /*top_controls_height=*/kControlsHeight,
      /*browser_controls_show_threshold=*/0.5f,
      /*browser_controls_hide_threshold=*/0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.0f, kControlsHeight));
  manager->ScrollEnd();
  EXPECT_FLOAT_EQ(manager->TopControlsShownRatio(), 0.0f);

  manager->ScrollBegin();

  // Scroll by a small amount that is not enough to show controls.
  manager->ScrollBy(gfx::Vector2dF(0.0f, -kControlsHeight * 0.01f));
  EXPECT_LT(manager->TopControlsShownRatio(), 1.0f);

  // ScrollEnd with latency compensation should show the controls if the
  // compensated delta is enough to show the controls.
  EXPECT_FALSE(manager->HasAnimation());
  manager->ScrollEnd(gfx::Vector2dF(0.0f, -kControlsHeight * 0.5f));
  EXPECT_TRUE(manager->HasAnimation());
  base::TimeTicks time = base::TimeTicks::Now();
  while (manager->HasAnimation()) {
    time = base::Milliseconds(100) + time;
    manager->Animate(time);
  }
  EXPECT_FALSE(manager->HasAnimation());
  EXPECT_FLOAT_EQ(manager->TopControlsShownRatio(), 1.0f);
}

class BrowserControlsOffsetManagerCancelAnimationTest : public testing::Test {
 public:
  BrowserControlsOffsetManagerCancelAnimationTest()
      : client_(100.0f, 0.5f, 0.5f) {}

 protected:
  MockBrowserControlsOffsetManagerClient client_;
};

TEST_F(BrowserControlsOffsetManagerCancelAnimationTest,
       HeightChangeCancelAnimationsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kBrowserControlsHeightChangeCancelAnimations);
  BrowserControlsOffsetManager* manager = client_.manager();

  manager->UpdateBrowserControlsState(BrowserControlsState::kHidden,
                                      BrowserControlsState::kBoth, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->HasAnimation());

  client_.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_TRUE(manager->HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCancelAnimationTest,
       HeightChangeCancelAnimationsEnabledDoNotCancelConstraintAnimation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kBrowserControlsHeightChangeCancelAnimations);
  BrowserControlsOffsetManager* manager = client_.manager();

  manager->UpdateBrowserControlsState(BrowserControlsState::kHidden,
                                      BrowserControlsState::kBoth, true,
                                      std::nullopt);
  EXPECT_TRUE(manager->HasAnimation());

  client_.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_TRUE(manager->HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCancelAnimationTest,
       HeightChangeCancelAnimationsEnabledWithAnimatedHeightChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kBrowserControlsHeightChangeCancelAnimations);
  BrowserControlsOffsetManager* manager = client_.manager();

  client_.SetBrowserControlsParams({150, 0, 0, 0, true, false});
  EXPECT_TRUE(manager->HasAnimation());

  client_.SetBrowserControlsParams({120, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCancelAnimationTest,
       HeightChangeCancelAnimationsEnabledNoAnimation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kBrowserControlsHeightChangeCancelAnimations);
  BrowserControlsOffsetManager* manager = client_.manager();

  client_.SetBrowserControlsParams({150, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());

  client_.SetBrowserControlsParams({120, 0, 0, 0, false, false});
  EXPECT_FALSE(manager->HasAnimation());
}

// TODO(b/501391526): Enable the tests on Linux TSAN once
// base::ScopedMockClockOverride is thread-safe.
#if !(BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER))
class BrowserControlsOffsetManagerSnapAnimationTest : public testing::Test {
 public:
  BrowserControlsOffsetManagerSnapAnimationTest()
      : client_(kControlsHeight, 0.5f, 0.5f) {
    feature_list_.InitAndEnableFeature(
        features::kBrowserControlsScrollSnapAnimation);
  }

 protected:
  // Enum to represent the animation direction of browser controls.
  enum class AnimationDirection {
    kNone,
    kShowingControls,
    kHidingControls,
  };

  // Helper class to start and end a scroll sequence and verify the expected
  // animation behavior at the end of the scroll.
  class ScrollSequence {
   public:
    ScrollSequence(BrowserControlsOffsetManagerSnapAnimationTest* test,
                   AnimationDirection scroll_end_animation_direction =
                       AnimationDirection::kNone)
        : test_(test),
          scroll_end_animation_direction_(scroll_end_animation_direction) {
      test_->client_.manager()->ScrollBegin();
    }

    ~ScrollSequence() {
      test_->client_.manager()->ScrollEnd();
      EXPECT_EQ(test_->client_.manager()->HasAnimation(),
                scroll_end_animation_direction_ != AnimationDirection::kNone);
      if (scroll_end_animation_direction_ != AnimationDirection::kNone) {
        test_->AnimateToCompletion(scroll_end_animation_direction_ ==
                                   AnimationDirection::kShowingControls);
      }
    }

   private:
    raw_ptr<BrowserControlsOffsetManagerSnapAnimationTest> test_;
    AnimationDirection scroll_end_animation_direction_;
  };

  void ScrollBy(float scroll_y,
                base::TimeDelta time_delta_from_previous_scroll_update =
                    base::Milliseconds(1),
                bool is_inertial = false) {
    constexpr base::TimeDelta kEpsilonTimeDelta = base::Microseconds(1);

    // Advance the mock clock to ensure that the second scroll update is not
    // coalesced with the first and is treated as a new sample. Also, split the
    // scroll delta into two parts since BrowserControlsOffsetManager requires
    // at least two samples in the scroll velocity tracker to trigger the
    // animation.
    mock_clock_.Advance(time_delta_from_previous_scroll_update -
                        kEpsilonTimeDelta);
    client_.ScrollVerticallyBy(scroll_y / 2, is_inertial);

    mock_clock_.Advance(kEpsilonTimeDelta);
    client_.ScrollVerticallyBy(scroll_y / 2, is_inertial);
  }

  // Returns assertion success if scrolling the client by the given scroll delta
  // triggered a snap animation, and failure otherwise. Also checks if the
  // triggered animation is configured correctly.
  testing::AssertionResult ScrollDidAnimate(
      float scroll_y,
      AnimationDirection animation_direction,
      base::TimeDelta time_delta_from_previous_scroll_update =
          base::Milliseconds(1),
      bool is_inertial = false) {
    BrowserControlsOffsetManager* manager = client_.manager();
    ScrollBy(scroll_y, time_delta_from_previous_scroll_update, is_inertial);

    if (!manager->HasAnimation()) {
      if (animation_direction == AnimationDirection::kNone) {
        return testing::AssertionSuccess();
      } else {
        return testing::AssertionFailure()
               << "No animation triggered for scroll delta " << scroll_y
               << " when a "
               << (animation_direction == AnimationDirection::kShowingControls
                       ? "show"
                       : "hide")
               << " animation was expected.";
      }
    } else {
      if (animation_direction == AnimationDirection::kNone) {
        return testing::AssertionFailure()
               << "Animation triggered for scroll delta " << scroll_y
               << " when no animation was expected.";
      } else {
        AnimateToCompletion(animation_direction ==
                            AnimationDirection::kShowingControls);
        return testing::AssertionSuccess();
      }
    }
  }

  testing::AssertionResult ScrollDidNotAnimate(
      float scroll_y,
      base::TimeDelta time_delta_from_previous_scroll_update =
          base::Milliseconds(1)) {
    return ScrollDidAnimate(scroll_y, AnimationDirection::kNone,
                            time_delta_from_previous_scroll_update);
  }

  void AnimateToCompletion(bool animate_to_show) {
    BrowserControlsOffsetManager* manager = client_.manager();
    ASSERT_EQ(manager->IsAnimatingToShowControls(), animate_to_show);
    const float final_shown_ratio =
        manager->IsAnimatingToShowControls() ? 1.0f : 0.0f;

    // Let the animation run to completion.
    base::TimeTicks time = base::TimeTicks::Now();
    while (manager->HasAnimation()) {
      time = base::Milliseconds(200) + time;
      manager->Animate(time);
    }
    ASSERT_FALSE(manager->HasAnimation());
    ASSERT_EQ(manager->TopControlsShownRatio(), final_shown_ratio);
    ASSERT_EQ(manager->BottomControlsShownRatio(), final_shown_ratio);
  }

  float MeasureScrollDeltaToHide(
      float step_size,
      base::TimeDelta interval_between_scroll_updates) {
    float scroll_delta = step_size;
    {
      ScrollSequence scroll_sequence(this);
      while (!ScrollDidAnimate(step_size, AnimationDirection::kHidingControls,
                               interval_between_scroll_updates)) {
        scroll_delta += step_size;
      }
    }
    return scroll_delta;
  }

  // Returns a large scroll delta that is guaranteed to trigger the snap
  // animation when the top of the page is in the can-hide region.
  float LargeScrollDelta() {
    return client_.manager()->SnapAnimationThreshold(/*slowness=*/1.0f);
  }

  constexpr static float kControlsHeight = 100.0f;

  MockBrowserControlsOffsetManagerClient client_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedMockClockOverride mock_clock_;
};

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       HideAnimationTriggeredOncePerScroll) {
  BrowserControlsOffsetManager* manager = client_.manager();

  // Start well inside in the can-hide region.
  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + 2 * LargeScrollDelta());

  {
    // Controls should hide at the end of this scroll sequence.
    ScrollSequence scroll_sequence(this, AnimationDirection::kHidingControls);
    // Simulate the user scrolling up and down in succession. The expected
    // behavior is:
    //   1. Scrolling down in the can-hide region should hide the browser
    //   controls.
    //   2. Scrolling up so that net scroll is equal to maximum trigger
    //   threshold should show controls.
    //   3. The controls cannot be hidden more than once per scroll, unless the
    //   scroll is inertial, so scrolling down normally should have no effect.
    EXPECT_TRUE(ScrollDidAnimate(LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
    EXPECT_TRUE(ScrollDidAnimate(-2 * LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
    EXPECT_TRUE(ScrollDidNotAnimate(2 * LargeScrollDelta()));
  }
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       ShowAnimationTriggeredMoreThanOncePerScroll) {
  BrowserControlsOffsetManager* manager = client_.manager();

  // Start in the can-hide region.
  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + 2 * LargeScrollDelta());

  {
    ScrollSequence scroll_sequence(this);
    // Hide the browser controls.
    EXPECT_TRUE(ScrollDidAnimate(LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
  }

  {
    ScrollSequence scroll_sequence(this);
    // Simulate the user scrolling up and down in succession. The expected
    // behavior is:
    //   1. Scrolling up in the can-hide region should show the browser
    //   controls.
    //   2. Scrolling down so that net scroll is equal to maximum trigger
    //   threshold should hide controls.
    //   3. The controls can be shown more than once per scroll, so scrolling
    //   up should show the controls again.
    //   4. The controls cannot be hidden more than once per scroll if the
    //   scroll is inertial, so an inertial scroll down should hide the
    //   controls.
    EXPECT_TRUE(ScrollDidAnimate(-LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
    EXPECT_TRUE(ScrollDidAnimate(2 * LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
    EXPECT_TRUE(ScrollDidAnimate(-2 * LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
  }
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       HideAnimationTriggeredMoreThanOncePerScrollIfInertial) {
  BrowserControlsOffsetManager* manager = client_.manager();

  // Start in the can-hide region.
  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + 2 * LargeScrollDelta());

  {
    ScrollSequence scroll_sequence(this);
    // Hide the browser controls.
    EXPECT_TRUE(ScrollDidAnimate(LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
  }

  {
    ScrollSequence scroll_sequence(this);
    // Simulate the user scrolling up and down in succession. The expected
    // behavior is:
    //   1. Scrolling up in the can-hide region should show the browser
    //   controls.
    //   2. Scrolling down so that net scroll is equal to maximum trigger
    //   threshold should hide controls.
    //   3. The controls can be shown more than once per scroll, so scrolling
    //   up should show the controls again.
    //   4. The controls can be hidden more than once per scroll if the scroll
    //   is inertial.
    EXPECT_TRUE(ScrollDidAnimate(-LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
    EXPECT_TRUE(ScrollDidAnimate(2 * LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
    EXPECT_TRUE(ScrollDidAnimate(-2 * LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
    EXPECT_TRUE(ScrollDidAnimate(2 * LargeScrollDelta(),
                                 AnimationDirection::kHidingControls,
                                 base::Milliseconds(1),
                                 /*is_inertial=*/true));
  }
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       ControlsHideOnlyInCanHideRegion) {
  BrowserControlsOffsetManager* manager = client_.manager();

  client_.SetViewportScrollOffset(0.0f, 0.0f);
  {
    ScrollSequence scroll_sequence(this);

    // Before the top of the page is in the can-hide region, the controls should
    // not be hidden.
    float can_hide_region_height_min =
        manager->SnapAnimationCanHideRegionHeight(0.0f);
    while (client_.ViewportScrollOffset().y() < can_hide_region_height_min) {
      EXPECT_TRUE(ScrollDidNotAnimate(1.0f));
    }

    // Once the top of the page is in the can-hide region, the controls should
    // be hidden.
    bool did_animate = false;
    float can_hide_region_height_max =
        manager->SnapAnimationCanHideRegionHeight(1.0f);
    while (client_.ViewportScrollOffset().y() <= can_hide_region_height_max) {
      if (ScrollDidAnimate(1.0f, AnimationDirection::kHidingControls)) {
        did_animate = true;
        break;
      }
    }
    EXPECT_TRUE(did_animate);
  }
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       CanHideRegionHeightIsSmallerForFasterScrolls) {
  constexpr base::TimeDelta kSlowScrollUpdateInterval = base::Milliseconds(1);
  constexpr base::TimeDelta kFastScrollUpdateInterval =
      kSlowScrollUpdateInterval / 2;

  BrowserControlsOffsetManager* manager = client_.manager();

  ASSERT_EQ(manager->TopControlsShownRatio(), 1.0f);

  float can_hide_region_height_slow_scroll = MeasureScrollDeltaToHide(
      /*step_size=*/1.0f,
      /*interval_between_scroll_updates=*/kSlowScrollUpdateInterval);
  EXPECT_GT(can_hide_region_height_slow_scroll, 0.0f);
  EXPECT_EQ(manager->TopControlsShownRatio(), 0.0f);

  // Scroll up to page top and reveal the controls.
  bool did_animate = false;
  {
    ScrollSequence scroll_sequence(this);
    while (client_.ViewportScrollOffset().y() > 0.0f) {
      did_animate |=
          ScrollDidAnimate(-std::min(1.0f, client_.ViewportScrollOffset().y()),
                           AnimationDirection::kShowingControls);
    }
  }
  EXPECT_TRUE(did_animate);
  ASSERT_EQ(manager->TopControlsShownRatio(), 1.0f);

  float can_hide_region_height_fast_scroll = MeasureScrollDeltaToHide(
      /*step_size=*/1.0f,
      /*interval_between_scroll_updates=*/kFastScrollUpdateInterval);
  EXPECT_GT(can_hide_region_height_fast_scroll, 0.0f);
  EXPECT_EQ(manager->TopControlsShownRatio(), 0.0f);

  EXPECT_GT(can_hide_region_height_slow_scroll,
            can_hide_region_height_fast_scroll);
  EXPECT_LT(can_hide_region_height_fast_scroll,
            manager->SnapAnimationCanHideRegionHeight(1.0f));
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       SnapAnimationThresholdIsSmallerForFasterScrolls) {
  constexpr base::TimeDelta kSlowScrollUpdateInterval = base::Milliseconds(1);
  constexpr base::TimeDelta kFastScrollUpdateInterval =
      kSlowScrollUpdateInterval / 2;

  BrowserControlsOffsetManager* manager = client_.manager();

  // Start well inside in the can-hide region.
  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + 2 * LargeScrollDelta());

  ASSERT_EQ(manager->TopControlsShownRatio(), 1.0f);
  float trigger_threshold_slow_scroll = MeasureScrollDeltaToHide(
      /*step_size=*/1.0f,
      /*interval_between_scroll_updates=*/kSlowScrollUpdateInterval);
  EXPECT_GT(trigger_threshold_slow_scroll, 0.0f);
  ASSERT_EQ(manager->TopControlsShownRatio(), 0.0f);

  {
    ScrollSequence scroll_sequence(this);
    // Show the browser controls.
    EXPECT_TRUE(ScrollDidAnimate(-LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
  }
  ASSERT_EQ(manager->TopControlsShownRatio(), 1.0f);

  float trigger_threshold_fast_scroll = MeasureScrollDeltaToHide(
      /*step_size=*/1.0f,
      /*interval_between_scroll_updates=*/kFastScrollUpdateInterval);
  EXPECT_GT(trigger_threshold_fast_scroll, 0.0f);
  ASSERT_EQ(manager->TopControlsShownRatio(), 0.0f);

  EXPECT_GT(trigger_threshold_slow_scroll, trigger_threshold_fast_scroll);
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       ControlsShowInAlwaysShownRegion) {
  BrowserControlsOffsetManager* manager = client_.manager();

  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + LargeScrollDelta());
  {
    ScrollSequence scroll_sequence(this);
    ASSERT_TRUE(ScrollDidAnimate(LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
  }

  // Scroll up in discrete scrolls until the top of the page is just outside the
  // always-shown region, lest the scroll-end processing should trigger the show
  // animation..
  while (client_.ViewportScrollOffset().y() >
         manager->SnapAnimationAlwaysShownRegionHeight() + 1.0f) {
    ScrollSequence scroll_sequence(this);
    EXPECT_TRUE(ScrollDidNotAnimate(-1.0f));
  }

  {
    ScrollSequence scroll_sequence(this);
    // Once in the always-shown region, scrolling up should show the controls.
    ASSERT_TRUE(ScrollDidAnimate(-1.0f, AnimationDirection::kShowingControls,
                                 base::Seconds(1)));
  }
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       ScrollEndAnimatesOnlyInDirectionOfScrollVelocity) {
  BrowserControlsOffsetManager* manager = client_.manager();

  // Start well inside in the can-hide region.
  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + 2 * LargeScrollDelta());

  {
    // Controls should not hide at the end of this scroll sequence since the
    // scroll velocity is on the opposite direction of hiding the controls.
    ScrollSequence scroll_sequence(this);
    EXPECT_TRUE(ScrollDidAnimate(LargeScrollDelta(),
                                 AnimationDirection::kHidingControls));
    EXPECT_TRUE(ScrollDidAnimate(-2 * LargeScrollDelta(),
                                 AnimationDirection::kShowingControls));
    EXPECT_TRUE(ScrollDidNotAnimate(2 * LargeScrollDelta()));
    EXPECT_TRUE(ScrollDidNotAnimate(-LargeScrollDelta()));
  }
}

TEST_F(BrowserControlsOffsetManagerSnapAnimationTest,
       ShowAnimationTriggeredWhenHideCompletesInAlwaysShownRegion) {
  BrowserControlsOffsetManager* manager = client_.manager();

  // Start well inside the can-hide region.
  client_.SetViewportScrollOffset(
      0.0f,
      manager->SnapAnimationCanHideRegionHeight(1.0f) + 2 * LargeScrollDelta());

  // Trigger a hide animation by scrolling down. `ScrollDidAnimate` is not used
  // here because we want to simulate the user scrolling up and down in
  // succession, and we need to be able to inspect the state of the controls
  // after the first scroll but before the second scroll is triggered.
  manager->ScrollBegin();
  ScrollBy(LargeScrollDelta(), base::Milliseconds(1), /*is_inertial=*/false);
  manager->ScrollEnd();

  // A hide animation should be running.
  ASSERT_TRUE(manager->HasAnimation());
  ASSERT_EQ(manager->IsAnimatingToShowControls(), false);
  ASSERT_EQ(manager->TopControlsShownRatio(), 1.0f);

  // Scroll up to the top of the page.
  manager->ScrollBegin();
  ScrollBy(-client_.ViewportScrollOffset().y(), base::Microseconds(10),
           /*is_inertial=*/false);
  manager->ScrollEnd();

  // A show animation should have been triggered immediately after the hide
  // animation completes since the top of the page is in the always-shown
  // region. Verify this by ensuring that the controls shown ratio decreases at
  // first and then increases to fully shown.
  bool hide_animation_completed = false;
  float prev_shown_ratio = manager->TopControlsShownRatio();
  while (manager->HasAnimation()) {
    // Slowly advance the animation to capture the inflection point where the
    // controls shown ratio starts increasing after the hide animation
    // completes.
    mock_clock_.Advance(base::Milliseconds(1));
    manager->Animate(base::TimeTicks::Now());

    float current_shown_ratio = manager->TopControlsShownRatio();
    if (hide_animation_completed) {
      EXPECT_GT(current_shown_ratio, prev_shown_ratio);
    } else {
      hide_animation_completed = (current_shown_ratio > prev_shown_ratio);
    }
    prev_shown_ratio = current_shown_ratio;
  }
  EXPECT_TRUE(hide_animation_completed);
  EXPECT_EQ(manager->TopControlsShownRatio(), 1.0f);
}

#endif  // !(BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER))

}  // namespace
}  // namespace cc
