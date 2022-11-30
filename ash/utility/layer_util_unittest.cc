// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/layer_util.h"

#include "base/cancelable_callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

class LayerUtilTest : public testing::Test {
 public:
  LayerUtilTest() = default;
  LayerUtilTest(const LayerUtilTest&) = delete;
  LayerUtilTest& operator=(const LayerUtilTest&) = delete;
  ~LayerUtilTest() override = default;

  // testing::Test:
  void SetUp() override {
    context_factories_ = std::make_unique<ui::TestContextFactories>(false);

    const gfx::Rect bounds(300, 300);
    host_.reset(ui::TestCompositorHost::Create(
        bounds, context_factories_->GetContextFactory()));
    host_->Show();

    compositor()->SetRootLayer(&root_);
  }
  void TearDown() override {
    host_.reset();
    context_factories_.reset();
  }

  void Advance(const base::TimeDelta& delta) {
    task_environment_.FastForwardBy(delta);
  }

  void GenerateOneFrame() { compositor()->ScheduleFullRedraw(); }

  ui::Compositor* compositor() { return host_->GetCompositor(); }
  ui::Layer* root_layer() { return &root_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};

  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<ui::TestCompositorHost> host_;
  ui::Layer root_;
};

}  // namespace

TEST_F(LayerUtilTest, CopyContentToExistingLayer) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ui::Layer layer1;
  layer1.SetBounds(gfx::Rect(100, 100));
  root_layer()->Add(&layer1);

  ui::Layer layer2;
  layer2.SetBounds(gfx::Rect(100, 100));
  root_layer()->Add(&layer2);

  {
    bool called = false;
    base::CancelableOnceCallback<void(ui::Layer**)> cancelable;
    cancelable.Reset(base::BindLambdaForTesting([&](ui::Layer** dummy) {
      called = true;
      *dummy = &layer2;
    }));
    CopyLayerContentToLayer(&layer1, cancelable.callback());

    GenerateOneFrame();
    Advance(base::Milliseconds(1000));
    EXPECT_TRUE(called);
  }

  // Test cancel scenario.
  {
    bool called = false;
    base::CancelableOnceCallback<void(ui::Layer**)> cancelable;
    cancelable.Reset(base::BindLambdaForTesting([&](ui::Layer** dummy) {
      called = true;
      *dummy = &layer2;
    }));

    CopyLayerContentToLayer(&layer1, cancelable.callback());
    cancelable.Cancel();

    GenerateOneFrame();
    Advance(base::Milliseconds(1000));
    EXPECT_FALSE(called);
  }
}

}  //  namespace ash
