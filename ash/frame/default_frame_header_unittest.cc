// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/default_frame_header.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_back_button.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/window_util.h"

using chromeos::DefaultFrameHeader;
using chromeos::FrameBackButton;
using chromeos::FrameCaptionButtonContainerView;
using chromeos::FrameHeader;
using chromeos::kFrameActiveColorKey;
using chromeos::kFrameInactiveColorKey;
using views::NonClientFrameView;
using views::Widget;

namespace ash {

class DefaultFrameHeaderTest : public AshTestBase {
 public:
  DefaultFrameHeaderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DefaultFrameHeaderTest(const DefaultFrameHeaderTest&) = delete;
  DefaultFrameHeaderTest& operator=(const DefaultFrameHeaderTest&) = delete;
  ~DefaultFrameHeaderTest() override = default;

  void AdvanceClock(base::TimeDelta delay) {
    task_environment()->AdvanceClock(delay);
    task_environment()->RunUntilIdle();
  }
};

// Ensure the title text is vertically aligned with the window icon.
TEST_F(DefaultFrameHeaderTest, TitleIconAlignment) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());
  views::StaticSizedView window_icon(gfx::Size(16, 16));
  window_icon.SetBounds(0, 0, 16, 16);
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));
  widget->Show();

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  frame_header.SetLeftHeaderView(&window_icon);
  frame_header.LayoutHeader();
  gfx::Rect title_bounds = frame_header.GetTitleBounds();
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            title_bounds.CenterPoint().y());
}

TEST_F(DefaultFrameHeaderTest, BackButtonAlignment) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());
  FrameBackButton back;

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  frame_header.SetBackButton(&back);
  frame_header.LayoutHeader();
  gfx::Rect title_bounds = frame_header.GetTitleBounds();
  // The back button should be positioned at the left edge, and
  // vertically centered.
  EXPECT_EQ(back.bounds().CenterPoint().y(), title_bounds.CenterPoint().y());
  EXPECT_EQ(0, back.bounds().x());
}

TEST_F(DefaultFrameHeaderTest, MinimumHeaderWidthRTL) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  std::unique_ptr<Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  frame_header.LayoutHeader();
  int ltr_minimum_width = frame_header.GetMinimumHeaderWidth();
  base::i18n::SetRTLForTesting(true);
  frame_header.LayoutHeader();
  int rtl_minimum_width = frame_header.GetMinimumHeaderWidth();
  EXPECT_EQ(ltr_minimum_width, rtl_minimum_width);
}

// Ensure the right frame colors are used.
TEST_F(DefaultFrameHeaderTest, FrameColors) {
  const auto win0_bounds = gfx::Rect{1, 2, 3, 4};
  auto win0 = CreateAppWindow(win0_bounds, chromeos::AppType::BROWSER);
  Widget* widget = Widget::GetWidgetForNativeWindow(win0.get());
  DefaultFrameHeader* frame_header =
      static_cast<DefaultFrameHeader*>(FrameHeader::Get(widget));
  // Check frame color is sensitive to mode.
  SkColor active = SkColorSetRGB(70, 70, 70);
  SkColor inactive = SkColorSetRGB(200, 200, 200);
  win0->SetProperty(kFrameActiveColorKey, active);
  win0->SetProperty(kFrameInactiveColorKey, inactive);
  frame_header->UpdateFrameColors();
  frame_header->mode_ = FrameHeader::MODE_ACTIVE;
  EXPECT_EQ(active, frame_header->GetCurrentFrameColor());
  frame_header->mode_ = FrameHeader::MODE_INACTIVE;
  EXPECT_EQ(inactive, frame_header->GetCurrentFrameColor());
  EXPECT_EQ(active, frame_header->active_frame_color_);

  // Update to the new value which has no blue, which should animate.
  frame_header->mode_ = FrameHeader::MODE_ACTIVE;
  SkColor new_active = SkColorSetRGB(70, 70, 0);
  win0->SetProperty(kFrameActiveColorKey, new_active);
  frame_header->UpdateFrameColors();

  // Now update to the new value which is full blue.
  SkColor new_new_active = SkColorSetRGB(70, 70, 255);
  win0->SetProperty(kFrameActiveColorKey, new_new_active);
  frame_header->UpdateFrameColors();

  // Again, GetCurrentFrameColor should return the target color.
  EXPECT_EQ(new_new_active, frame_header->GetCurrentFrameColor());
}

namespace {

class LayerDestroyedChecker : public ui::LayerObserver {
 public:
  explicit LayerDestroyedChecker(ui::Layer* layer) { layer->AddObserver(this); }
  LayerDestroyedChecker(const LayerDestroyedChecker&) = delete;
  LayerDestroyedChecker& operator=(const LayerDestroyedChecker&) = delete;
  ~LayerDestroyedChecker() override = default;

  void LayerDestroyed(ui::Layer* layer) override {
    layer->RemoveObserver(this);
    destroyed_ = true;
  }
  bool destroyed() const { return destroyed_; }

 private:
  bool destroyed_ = false;
};

}  // namespace

// A class to wait until hthe frame header is painted.
class FramePaintWaiter : public ui::CompositorObserver {
 public:
  explicit FramePaintWaiter(aura::Window* window)
      : frame_header_(
            FrameHeader::Get(Widget::GetWidgetForNativeWindow(window))) {
    frame_header_->view()->GetWidget()->GetCompositor()->AddObserver(this);
  }
  FramePaintWaiter(const FramePaintWaiter&) = delete;
  FramePaintWaiter& operator=(FramePaintWaiter&) = delete;
  ~FramePaintWaiter() override {
    frame_header_->view()->GetWidget()->GetCompositor()->RemoveObserver(this);
  }

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    if (frame_header_->painted_)
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  raw_ptr<FrameHeader> frame_header_ = nullptr;
};

TEST_F(DefaultFrameHeaderTest, DeleteDuringAnimation) {
  const auto bounds = gfx::Rect(100, 100);
  auto win0 = CreateAppWindow(bounds, chromeos::AppType::BROWSER);
  auto win1 = CreateAppWindow(bounds, chromeos::AppType::BROWSER);

  Widget* widget = Widget::GetWidgetForNativeWindow(win0.get());
  EXPECT_TRUE(FrameHeader::Get(widget));

  EXPECT_TRUE(wm::IsActiveWindow(win1.get()));

  // Waits until `FrameHeader` gets painted.
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(win0->GetHost()->compositor()));

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  wm::ActivateWindow(win0.get());

  auto* frame_view = NonClientFrameViewAsh::Get(win0.get());
  auto* animating_layer_holding_view = frame_view->children()[0].get();
  EXPECT_TRUE(views::IsViewClass<chromeos::FrameHeader::FrameAnimatorView>(
      animating_layer_holding_view));
  ASSERT_TRUE(animating_layer_holding_view->layer());
  ASSERT_GT(animating_layer_holding_view->layer()->parent()->children().size(),
            2u);
  auto* animating_layer =
      animating_layer_holding_view->layer()->parent()->children()[0].get();
  EXPECT_EQ(ui::LAYER_TEXTURED, animating_layer->type());
  EXPECT_TRUE(base::Contains(animating_layer->name(), ":Old"));
  EXPECT_TRUE(animating_layer->GetAnimator()->is_animating());

  LayerDestroyedChecker checker(animating_layer);

  win0.reset();

  EXPECT_TRUE(checker.destroyed());
}

// Make sure that the animation is canceled when resized.
TEST_F(DefaultFrameHeaderTest, ResizeAndReorderDuringAnimation) {
  const auto bounds = gfx::Rect(100, 100);
  auto win_0 = CreateAppWindow(bounds, chromeos::AppType::BROWSER);
  auto win_1 = CreateAppWindow(bounds, chromeos::AppType::BROWSER);

  EXPECT_TRUE(wm::IsActiveWindow(win_1.get()));

  // Waits until `FrameHeader` gets painted.
  EXPECT_TRUE(
      ui::WaitForNextFrameToBePresented(win_0->GetHost()->compositor()));

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* frame_view_0 = NonClientFrameViewAsh::Get(win_0.get());
  auto* animating_layer_holding_view_0 = frame_view_0->children()[0].get();
  EXPECT_TRUE(views::IsViewClass<chromeos::FrameHeader::FrameAnimatorView>(
      animating_layer_holding_view_0));
  size_t original_layers_count_0 =
      animating_layer_holding_view_0->layer()->parent()->children().size();

  auto* frame_view_1 = NonClientFrameViewAsh::Get(win_1.get());
  auto* extra_view_1 =
      frame_view_1->AddChildView(std::make_unique<views::View>());

  auto* animating_layer_holding_view_1 = frame_view_1->children()[0].get();
  EXPECT_TRUE(views::IsViewClass<chromeos::FrameHeader::FrameAnimatorView>(
      animating_layer_holding_view_1));
  size_t original_layers_count_1 =
      animating_layer_holding_view_1->layer()->parent()->children().size();

  wm::ActivateWindow(win_0.get());

  {
    // Resize during animation
    EXPECT_EQ(
        animating_layer_holding_view_0->layer()->parent()->children().size(),
        original_layers_count_0 + 1);
    auto* animating_layer =
        animating_layer_holding_view_0->layer()->parent()->children()[0].get();
    EXPECT_TRUE(animating_layer->GetAnimator()->is_animating());

    LayerDestroyedChecker checker(animating_layer);

    win_0->SetBounds(gfx::Rect(200, 200));

    // Animating layer shuld have been removed.
    EXPECT_EQ(
        animating_layer_holding_view_0->layer()->parent()->children().size(),
        original_layers_count_0);
    EXPECT_TRUE(checker.destroyed());
  }

  {
    // win_1 should still be animating.
    EXPECT_EQ(
        animating_layer_holding_view_1->layer()->parent()->children().size(),
        original_layers_count_1 + 1);
    auto* animating_layer =
        animating_layer_holding_view_1->layer()->parent()->children()[0].get();
    EXPECT_TRUE(animating_layer->GetAnimator()->is_animating());
    LayerDestroyedChecker checker(animating_layer);

    // Change the view's stacking order should stop the animation.
    ASSERT_EQ(3u, frame_view_1->children().size());
    frame_view_1->ReorderChildView(extra_view_1, 0);

    EXPECT_EQ(
        animating_layer_holding_view_1->layer()->parent()->children().size(),
        original_layers_count_1);
    EXPECT_TRUE(checker.destroyed());
  }
}

// Make sure that the animation request while animating will not
// create another animation.
TEST_F(DefaultFrameHeaderTest, AnimateDuringAnimation) {
  const auto bounds = gfx::Rect(100, 100);
  auto win_0 = CreateAppWindow(bounds, chromeos::AppType::BROWSER);
  // A frame will not animate until it is painted first.
  FramePaintWaiter(win_0.get()).Wait();

  auto* widget = Widget::GetWidgetForNativeWindow(win_0.get());

  auto lock = widget->LockPaintAsActive();
  auto win_1 = CreateAppWindow(bounds, chromeos::AppType::BROWSER);
  FramePaintWaiter(win_1.get()).Wait();

  EXPECT_TRUE(wm::IsActiveWindow(win_1.get()));

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto layer_bounds = win_0->layer()->bounds();
  lock.reset();
  win_1.reset();
  EXPECT_TRUE(wm::IsActiveWindow(win_0.get()));
  // Makes sure that the layer has full damaged bounds.
  EXPECT_TRUE(win_0->layer()->damaged_region().Contains(layer_bounds));
}

// Ensure that the number of frame color changes is recorded as metrics.
TEST_F(DefaultFrameHeaderTest, FrameColorChangeMetrics) {
  const auto app_type = chromeos::AppType::ARC_APP;
  auto win0 = CreateAppWindow(gfx::Rect(300, 300), app_type);
  Widget* widget = Widget::GetWidgetForNativeWindow(win0.get());
  DefaultFrameHeader* frame_header =
      static_cast<DefaultFrameHeader*>(FrameHeader::Get(widget));

  const auto frame_color_change_histogram =
      chromeos::FrameColorMetricsHelper::GetFrameColorChangeHistogramName(
          app_type);
  base::HistogramTester histogram_tester;

  win0->SetProperty(kFrameActiveColorKey, SkColorSetRGB(70, 70, 70));
  win0->SetProperty(kFrameInactiveColorKey, SkColorSetRGB(70, 70, 70));
  frame_header->UpdateFrameColors();

  constexpr base::TimeDelta kFrameColorTracingTime = base::Seconds(3);
  // Advances the mock clock in the task environment because the metrics is
  // recorded `kFrameColorTracingTime` after the `frame_header` is instantiated.
  AdvanceClock(kFrameColorTracingTime);

  histogram_tester.ExpectTotalCount(frame_color_change_histogram, 1);

  // The recorded number of frame color changes should be at least 1.
  EXPECT_GE(histogram_tester.GetAllSamples(frame_color_change_histogram)[0].min,
            1);
}

}  // namespace ash
