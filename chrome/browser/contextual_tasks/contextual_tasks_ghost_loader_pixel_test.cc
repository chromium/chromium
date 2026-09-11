// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ghost_loader_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace contextual_tasks {
namespace {

// Screen sizes to test responsive layout of the ghost loader:
// - Narrow (360px): Default side panel content width
//   (SidePanelEntry::kSidePanelDefaultContentWidth) and below the 420px wrap
//   threshold (2 * 184px + 12px gap + 40px padding), verifying the
//   single-column layout where cards expand to 100% width. Also aligns with the
//   WebUI breakpoint (max-width: 440px).
// - Regular (440px): Standard side panel width above the 420px threshold and
//   below the 654px max-width, verifying the two-column side-by-side card
//   layout.
// - Wide (800px): Wide side panel width above the ghost loader max-width of
//   654px and the WebUI breakpoint (min-width: 789px), verifying that the
//   component is clamped to 654px and centered horizontally.
constexpr gfx::Size kNarrowSize{360, 600};
constexpr gfx::Size kRegularSize{440, 600};
constexpr gfx::Size kWideSize{800, 600};

const std::vector<PixelTestParam>& GetTestParams() {
  static const base::NoDestructor<std::vector<PixelTestParam>> kTestParams(
      std::vector<PixelTestParam>{
          {.test_suffix = "Regular", .window_size = kRegularSize},
          {.test_suffix = "DarkTheme",
           .use_dark_theme = true,
           .window_size = kRegularSize},
          {.test_suffix = "Narrow", .window_size = kNarrowSize},
          {.test_suffix = "Narrow_DarkTheme",
           .use_dark_theme = true,
           .window_size = kNarrowSize},
          {.test_suffix = "Wide", .window_size = kWideSize},
          {.test_suffix = "Wide_DarkTheme",
           .use_dark_theme = true,
           .window_size = kWideSize},
      });
  return *kTestParams;
}

std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<PixelTestParam>& info) {
  return info.param.test_suffix;
}

// Pixel test verifying the WebUI Contextual Tasks ghost loader rendering in
// both light and dark themes.
class ContextualTasksGhostLoaderPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public ::testing::WithParamInterface<PixelTestParam> {
 public:
  ContextualTasksGhostLoaderPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasks,
                              kContextualTasksSidePanelRearchitecture},
        /*disabled_features=*/{});
  }
  ~ContextualTasksGhostLoaderPixelTest() override = default;

  void ShowUi(const std::string& name) override {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    const gfx::Size size = GetParam().window_size.value_or(kRegularSize);
    params.bounds = gfx::Rect(gfx::Point(), size);
    widget_->Init(std::move(params));

    auto ghost_loader =
        std::make_unique<ContextualTasksGhostLoaderView>(GetProfile());
    auto* ghost_loader_ptr = widget_->SetContentsView(std::move(ghost_loader));
    widget_->Show();

    ASSERT_TRUE(content::WaitForLoadStop(ghost_loader_ptr->web_contents()));
    EXPECT_EQ(true, content::EvalJs(ghost_loader_ptr->web_contents(),
                                    R"(
        (async () => {
          await customElements.whenDefined('ghost-loader');
          const el = document.querySelector('ghost-loader');
          if (!el) {
            return false;
          }
          await el.updateComplete;
          const container = el.shadowRoot.querySelector('.container');
          if (!container) {
            return false;
          }
          const anims = container.getAnimations({subtree: true});
          for (const anim of anims) {
            anim.currentTime = 0;
            anim.pause();
          }
          const style = document.createElement('style');
          style.textContent =
              '*, *::before, *::after { '
              'animation-play-state: paused !important; }';
          el.shadowRoot.appendChild(style);
          await new Promise(resolve => {
            requestAnimationFrame(() => requestAnimationFrame(resolve));
          });
          return true;
        })();
        )")
                        .ExtractBool());
  }

  bool VerifyUi() override {
    if (!widget_) {
      return false;
    }
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(widget_.get(), test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void DismissUi() override { widget_.reset(); }

  void TearDownOnMainThread() override {
    DismissUi();
    ProfilesPixelTestBaseT<UiBrowserTest>::TearDownOnMainThread();
  }

  void WaitForUserDismissal() override {
    if (!widget_) {
      return;
    }
    views::test::WidgetDestroyedWaiter waiter(widget_.get());
    waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
  std::unique_ptr<views::Widget> widget_;
};

IN_PROC_BROWSER_TEST_P(ContextualTasksGhostLoaderPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ContextualTasksGhostLoaderPixelTest,
                         testing::ValuesIn(GetTestParams()),
                         &ParamToTestSuffix);

}  // namespace
}  // namespace contextual_tasks
