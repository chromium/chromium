// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/logging.h"
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
      : host_impl_(&task_runner_provider_,
                   &task_graph_runner_),
        redraw_needed_(false),
        update_draw_properties_needed_(false),
        bottom_controls_height_(0.f),
        top_controls_shown_ratio_(1.f),
        top_controls_height_(top_controls_height),
        browser_controls_show_threshold_(browser_controls_show_threshold),
        browser_controls_hide_threshold_(browser_controls_hide_threshold) {
    active_tree_ = std::make_unique<LayerTreeImpl>(
        &host_impl_, new SyncedProperty<ScaleGroup>, new SyncedBrowserControls,
        new SyncedElasticOverscroll);
    root_scroll_layer_ = LayerImpl::Create(active_tree_.get(), 1);
  }

  ~MockBrowserControlsOffsetManagerClient() override = default;

  void DidChangeBrowserControlsPosition() override {
    redraw_needed_ = true;
    update_draw_properties_needed_ = true;
  }

  bool HaveRootScrollNode() const override { return true; }

  float BottomControlsHeight() const override {
    return bottom_controls_height_;
  }

  float TopControlsHeight() const override { return top_controls_height_; }

  void SetCurrentBrowserControlsShownRatio(float ratio) override {
    ASSERT_FALSE(std::isnan(ratio));
    ASSERT_FALSE(ratio == std::numeric_limits<float>::infinity());
    ASSERT_FALSE(ratio == -std::numeric_limits<float>::infinity());
    ratio = std::max(ratio, 0.f);
    ratio = std::min(ratio, 1.f);
    top_controls_shown_ratio_ = ratio;
  }

  float CurrentBrowserControlsShownRatio() const override {
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

  void SetBrowserControlsHeight(float height) { top_controls_height_ = height; }

  void SetBottomControlsHeight(float height) {
    bottom_controls_height_ = height;
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

  float bottom_controls_height_;
  float top_controls_shown_ratio_;
  float top_controls_height_;
  float browser_controls_show_threshold_;
  float browser_controls_hide_threshold_;
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

  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     BottomControlsPartialShownHideAnimation) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  client.SetBottomControlsHeight(100.f);
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

  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->BottomControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->BottomControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
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

  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(100.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     BottomControlsPartialShownShowAnimation) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  client.SetBottomControlsHeight(100.f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(0.8f, manager->BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->BottomControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->BottomControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
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
  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
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
  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
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
  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
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
  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
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

  EXPECT_TRUE(manager->has_animation());
}

TEST(BrowserControlsOffsetManagerTest, PinchBeginStartsAnimationIfNecessary) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_FLOAT_EQ(-100.f, manager->ControlsTopOffset());

  manager->PinchBegin();
  EXPECT_FALSE(manager->has_animation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->has_animation());

  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_FLOAT_EQ(-85.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(15.f, manager->ContentTopOffset());

  manager->PinchBegin();
  EXPECT_TRUE(manager->has_animation());

  base::TimeTicks time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  float previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->has_animation());

  manager->ScrollBy(gfx::Vector2dF(0.f, -55.f));
  EXPECT_FLOAT_EQ(-45.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(55.f, manager->ContentTopOffset());
  EXPECT_FALSE(manager->has_animation());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->has_animation());

  time = base::TimeTicks::Now();

  // First animate will establish the animaion.
  previous = manager->TopControlsShownRatio();
  manager->Animate(time);
  EXPECT_EQ(manager->TopControlsShownRatio(), previous);

  while (manager->has_animation()) {
    previous = manager->TopControlsShownRatio();
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->TopControlsShownRatio(), previous);
  }
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
}

TEST(BrowserControlsOffsetManagerTest,
     HeightChangeMaintainsFullyVisibleControls) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();

  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());

  client.SetBrowserControlsHeight(100.f);
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(100.f, manager->TopControlsHeight());
  EXPECT_FLOAT_EQ(0, manager->ControlsTopOffset());

  client.SetBrowserControlsHeight(50.f);
  EXPECT_FALSE(manager->has_animation());
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

  client.SetBrowserControlsHeight(50.f);
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(-50.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());

  client.SetBrowserControlsHeight(0.f);
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
}

TEST(BrowserControlsOffsetManagerTest, ScrollByWithZeroHeightControlsIsNoop) {
  MockBrowserControlsOffsetManagerClient client(0.f, 0.5f, 0.5f);
  BrowserControlsOffsetManager* manager = client.manager();
  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kBoth, false);

  manager->ScrollBegin();
  gfx::Vector2dF pending = manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_FLOAT_EQ(20.f, pending.y());
  EXPECT_FLOAT_EQ(0.f, manager->ControlsTopOffset());
  EXPECT_FLOAT_EQ(0.f, manager->ContentTopOffset());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBrowserControlsShownRatio());
  manager->ScrollEnd();
}

TEST(BrowserControlsOffsetManagerTest, ScrollThenRestoreBottomControls) {
  MockBrowserControlsOffsetManagerClient client(100.f, 0.5f, 0.5f);
  client.SetBottomControlsHeight(100.f);
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
  client.SetBottomControlsHeight(100.f);
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
  client.SetBottomControlsHeight(100.f);
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
  client.SetBottomControlsHeight(100.f);
  BrowserControlsOffsetManager* manager = client.manager();
  EXPECT_FLOAT_EQ(1.f, client.CurrentBrowserControlsShownRatio());

  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kHidden, true);
  EXPECT_TRUE(manager->has_animation());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBrowserControlsShownRatio());

  manager->UpdateBrowserControlsState(BrowserControlsState::kBoth,
                                      BrowserControlsState::kShown, true);
  EXPECT_FALSE(manager->has_animation());
  EXPECT_FLOAT_EQ(1.f, client.CurrentBrowserControlsShownRatio());
}

}  // namespace
}  // namespace cc
