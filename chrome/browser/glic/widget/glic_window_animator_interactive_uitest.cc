// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

class GlicWindowAnimatorUiTest : public test::InteractiveGlicTest {
 public:
  GlicWindowAnimatorUiTest() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{},
        /*disabled_features=*/{{features::kGlicUserResize}});
  }

  auto GetTargetBounds() {
    return Do([this]() {
      target_bounds_ =
          window_controller().window_animator()->GetCurrentTargetBounds();
    });
  }

  gfx::Rect GetWidgetBounds() {
    return window_controller().GetGlicWidget()->GetWindowBoundsInScreen();
  }

  auto CheckWidgetMoved(bool expect_bounds) {
    return CheckResult(
        [&]() { return target_bounds_.origin() == GetWidgetBounds().origin(); },
        expect_bounds, "CheckWidgetMoved");
  }

  auto WaitForAnimationStarted() {
    return Steps(InAnyContext(
        ObserveState(test::internal::kGlicWindowControllerState,
                     std::ref(window_controller())),
        WaitForState(test::internal::kGlicWindowControllerState,
                     GlicWindowController::State::kOpenAnimation)));
  }

  auto WaitUntilAnimationFinished() {
    return Steps(InAnyContext(
        WaitForState(test::internal::kGlicWindowControllerState,
                     GlicWindowController::State::kOpen),
        StopObservingState(test::internal::kGlicWindowControllerState)));
  }

  auto OpenDetached() {
    return Do([this]() { window_controller().ShowDetachedForTesting(); });
  }

 private:
  gfx::Rect target_bounds_;
  base::test::ScopedFeatureList features_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(GlicWindowAnimatorUiTest, OpenDetachedAnimationRuns) {
  RunTestSequence(InAnyContext(Steps(OpenDetached(), WaitForAnimationStarted(),
                                     WaitUntilAnimationFinished())));
}

IN_PROC_BROWSER_TEST_F(GlicWindowAnimatorUiTest,
                       DISABLED_OpenDetachedAnimationBoundsChanges) {
  // Expect widget animates (flies) down during detached open animation.
  RunTestSequence(InAnyContext(
      Steps(OpenDetached(), WaitForAnimationStarted(), GetTargetBounds(),
            WaitUntilAnimationFinished(), CheckWidgetMoved(true))));
}

}  // namespace glic
