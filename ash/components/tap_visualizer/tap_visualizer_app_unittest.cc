// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/tap_visualizer/tap_visualizer_app.h"

#include "ash/components/tap_visualizer/tap_renderer.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/view.h"

namespace tap_visualizer {
namespace {
constexpr int64_t kFirstDisplayId = 111;
constexpr int64_t kSecondDisplayId = 222;
}  // namespace

class TapVisualizerAppTestApi {
 public:
  explicit TapVisualizerAppTestApi(TapVisualizerApp* app) : app_(app) {}
  ~TapVisualizerAppTestApi() = default;

  void Start() { app_->Start(); }

  bool HasRendererForDisplay(int64_t display_id) {
    return base::ContainsKey(app_->display_id_to_renderer_, display_id);
  }

  views::Widget* GetWidgetForDisplay(int64_t display_id) {
    return app_->display_id_to_renderer_[display_id]->widget_.get();
  }

 private:
  TapVisualizerApp* app_;

  DISALLOW_COPY_AND_ASSIGN(TapVisualizerAppTestApi);
};

class TapVisualizerAppTest : public aura::test::AuraTestBase {
 public:
  TapVisualizerAppTest() = default;
  ~TapVisualizerAppTest() override = default;

  // aura::test::AuraTestBase:
  void SetUp() override {
    screen_ = std::make_unique<display::ScreenBase>();
    display::Screen::SetScreenInstance(screen_.get());
    screen_->display_list().AddDisplay(
        display::Display(kFirstDisplayId, gfx::Rect(0, 0, 800, 600)),
        display::DisplayList::Type::PRIMARY);
    EnableMusWithTestWindowTree();
    AuraTestBase::SetUp();
    // Create a MusClient using the AuraTestBase's TestWindowTreeClient,
    // which does not connect to a window service.
    views::MusClient::InitParams params;
    params.create_cursor_factory = false;
    params.create_wm_state = false;
    params.window_tree_client = window_tree_client_impl();
    mus_client_ = std::make_unique<views::MusClient>(params);
    ASSERT_TRUE(views::MusClient::Exists());
  }

  void TearDown() override {
    AuraTestBase::TearDown();
    display::Screen::SetScreenInstance(nullptr);
  }

 protected:
  views::TestViewsDelegate views_delegate_;
  std::unique_ptr<display::ScreenBase> screen_;
  std::unique_ptr<views::MusClient> mus_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TapVisualizerAppTest);
};

TEST_F(TapVisualizerAppTest, Basics) {
  // Simulate the service starting.
  TapVisualizerApp app;
  TapVisualizerAppTestApi test_api(&app);
  test_api.Start();

  // A fullscreen widget is created.
  views::Widget* widget = test_api.GetWidgetForDisplay(kFirstDisplayId);
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsFullscreen());

  // No touch point views have been created yet.
  views::View* contents = widget->GetContentsView();
  EXPECT_EQ(0, contents->child_count());

  // Simulate a touch tap.
  ui::TouchEvent tap(
      ui::ET_TOUCH_PRESSED, gfx::Point(1, 1), base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  widget->GetNativeWindow()->env()->NotifyEventObservers(tap);

  // A touch point view was created.
  EXPECT_EQ(1, contents->child_count());
}

TEST_F(TapVisualizerAppTest, MultiDisplay) {
  // Add a second display on the right.
  screen_->display_list().AddDisplay(
      display::Display(kSecondDisplayId, gfx::Rect(801, 0, 800, 600)),
      display::DisplayList::Type::NOT_PRIMARY);

  // Simulate the service starting.
  TapVisualizerApp app;
  TapVisualizerAppTestApi test_api(&app);
  test_api.Start();

  // Two renderers are created.
  EXPECT_TRUE(test_api.HasRendererForDisplay(kFirstDisplayId));
  EXPECT_TRUE(test_api.HasRendererForDisplay(kSecondDisplayId));

  // Simulate a touch tap on the second display.
  ui::TouchEvent tap(
      ui::ET_TOUCH_PRESSED, gfx::Point(802, 1), base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  views::Widget* widget1 = test_api.GetWidgetForDisplay(kFirstDisplayId);
  views::Widget* widget2 = test_api.GetWidgetForDisplay(kSecondDisplayId);
  EXPECT_EQ(widget1->GetNativeWindow()->env(),
            widget2->GetNativeWindow()->env());
  widget1->GetNativeWindow()->env()->NotifyEventObservers(tap);

  // A touch point view was created on the second display.
  EXPECT_EQ(0, widget1->GetContentsView()->child_count());
  EXPECT_EQ(1, widget2->GetContentsView()->child_count());

  // Disconnect the second display.
  screen_->display_list().RemoveDisplay(kSecondDisplayId);

  // Only the first display has a renderer.
  EXPECT_TRUE(test_api.HasRendererForDisplay(kFirstDisplayId));
  EXPECT_FALSE(test_api.HasRendererForDisplay(kSecondDisplayId));
}

}  // namespace tap_visualizer
