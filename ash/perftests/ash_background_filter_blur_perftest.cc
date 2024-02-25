// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/lap_timer.h"
#include "testing/perf/perf_test.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"

namespace ash {

namespace {

// TODO(wutao): On chromeos_linux builds, the tests only run with
// use_ozone = false.
class AshBackgroundFilterBlurPerfTest : public AshTestBase {
 public:
  AshBackgroundFilterBlurPerfTest() : timer_(0, base::TimeDelta(), 1) {}

  AshBackgroundFilterBlurPerfTest(const AshBackgroundFilterBlurPerfTest&) =
      delete;
  AshBackgroundFilterBlurPerfTest& operator=(
      const AshBackgroundFilterBlurPerfTest&) = delete;

  ~AshBackgroundFilterBlurPerfTest() override = default;

  // AshTestBase:
  void SetUp() override;

 protected:
  std::unique_ptr<ui::Layer> CreateSolidColorLayer(SkColor color);

  void WithBoundsChange(ui::Layer* layer,
                        int num_iteration,
                        const std::string& test_name);

  void WithOpacityChange(ui::Layer* layer,
                         int num_iteration,
                         const std::string& test_name);

  std::unique_ptr<ui::Layer> background_layer_;

  std::unique_ptr<ui::Layer> blur_layer_;

 private:
  raw_ptr<ui::Layer> root_layer_ = nullptr;

  raw_ptr<ui::Compositor> compositor_ = nullptr;

  base::LapTimer timer_;
};

void AshBackgroundFilterBlurPerfTest::SetUp() {
  AshTestBase::SetUp();

  // This is for consistency even if the default display size changed.
  UpdateDisplay("800x600");
  root_layer_ = Shell::GetAllRootWindows()[0]->layer();
  compositor_ = root_layer_->GetCompositor();
  background_layer_ = CreateSolidColorLayer(SK_ColorGREEN);
  blur_layer_ = CreateSolidColorLayer(SK_ColorBLACK);
}

std::unique_ptr<ui::Layer>
AshBackgroundFilterBlurPerfTest::CreateSolidColorLayer(SkColor color) {
  std::unique_ptr<ui::Layer> layer =
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  layer->SetBounds(root_layer_->bounds());
  layer->SetColor(color);
  root_layer_->Add(layer.get());
  root_layer_->StackAtTop(layer.get());
  return layer;
}

void AshBackgroundFilterBlurPerfTest::WithBoundsChange(
    ui::Layer* layer,
    int num_iteration,
    const std::string& test_name) {
  const gfx::Rect init_bounds = layer->GetTargetBounds();
  // Wait for a DidCommit before starts the loop, and do not measure the last
  // iteration of the loop.
  ui::DrawWaiterForTest::WaitForCommit(compositor_);
  timer_.Reset();
  for (int i = 1; i <= num_iteration + 1; ++i) {
    float fraction = (static_cast<float>(i) / num_iteration);
    const gfx::Rect bounds =
        gfx::Rect(0, 0, static_cast<int>(init_bounds.width() * fraction),
                  static_cast<int>(init_bounds.height() * fraction));
    layer->SetBounds(bounds);
    ui::DrawWaiterForTest::WaitForCommit(compositor_);
    if (i <= num_iteration)
      timer_.NextLap();
  }
  perf_test::PrintResult("AshBackgroundFilterBlurPerfTest", std::string(),
                         test_name, timer_.LapsPerSecond(), "runs/s", true);
}

void AshBackgroundFilterBlurPerfTest::WithOpacityChange(
    ui::Layer* layer,
    int num_iteration,
    const std::string& test_name) {
  float init_opacity = layer->GetTargetOpacity();
  // Wait for a DidCommit before starts the loop, and do not measure the last
  // iteration of the loop.
  ui::DrawWaiterForTest::WaitForCommit(compositor_);
  timer_.Reset();
  for (int i = 1; i <= num_iteration + 1; ++i) {
    float fraction = (static_cast<float>(i) / num_iteration);
    float opacity = std::min(1.0f, init_opacity * fraction);
    layer->SetOpacity(opacity);
    ui::DrawWaiterForTest::WaitForCommit(compositor_);
    if (i <= num_iteration)
      timer_.NextLap();
  }
  perf_test::PrintResult("AshBackgroundFilterBlurPerfTest", std::string(),
                         test_name, timer_.LapsPerSecond(), "runs/s", true);
}

TEST_F(AshBackgroundFilterBlurPerfTest, NoBlurBackgroundLayerBoundsChange) {
  WithBoundsChange(background_layer_.get(), 100,
                   "no_blur_background_layer_bounds_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, NoBlurBlurLayerBoundsChange) {
  WithBoundsChange(blur_layer_.get(), 100, "no_blur_blur_layer_bounds_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, BackgroundLayerBoundsChange) {
  blur_layer_->SetBackgroundBlur(10.f);
  WithBoundsChange(background_layer_.get(), 100,
                   "background_layer_bounds_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, BlurLayerBoundsChange) {
  blur_layer_->SetBackgroundBlur(10.f);
  WithBoundsChange(blur_layer_.get(), 100, "blur_layer_bounds_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, NoBlurBackgroundLayerOpacityChange) {
  WithOpacityChange(background_layer_.get(), 100,
                    "no_blur_background_layer_opacity_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, NoBlurBlurLayerOpacityChange) {
  WithOpacityChange(blur_layer_.get(), 100,
                    "no_blur_blur_layer_opacity_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, BackgroundLayerOpacityChange) {
  blur_layer_->SetBackgroundBlur(10.f);
  WithOpacityChange(background_layer_.get(), 100,
                    "background_layer_opacity_change");
}

TEST_F(AshBackgroundFilterBlurPerfTest, BlurLayerOpacityChange) {
  blur_layer_->SetBackgroundBlur(10.f);
  WithOpacityChange(blur_layer_.get(), 100, "blur_layer_opacity_change");
}

}  // namespace
}  // namespace ash
