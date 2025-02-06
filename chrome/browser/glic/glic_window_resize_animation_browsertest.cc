// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_resize_animation.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace glic {
namespace {

constexpr int kTestAnimationDuration = 300;

class GlicWindowResizeAnimationTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Mark the glic FRE as accepted by default.
    // TODO(cuianthony): Move this logic to glic_test_util.h after
    // https://chromium-review.googlesource.com/c/chromium/src/+/6197534 lands.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kGlicCompletedFre, true);
  }

  void ExpectRectBetween(const gfx::Rect& current_rect,
                         const gfx::Rect& initial_rect,
                         const gfx::Rect& final_rect) {
    ExpectValueBetween(current_rect.x(), initial_rect.x(), final_rect.x());
    ExpectValueBetween(current_rect.y(), initial_rect.y(), final_rect.y());
    ExpectValueBetween(current_rect.width(), initial_rect.width(),
                       final_rect.width());
    ExpectValueBetween(current_rect.height(), initial_rect.height(),
                       final_rect.height());
  }

 protected:
  void ExpectValueBetween(int current, int initial, int final) {
    if (initial < final) {
      EXPECT_TRUE(initial <= current);
      EXPECT_TRUE(current <= final);
    } else {
      EXPECT_TRUE(final <= current);
      EXPECT_TRUE(current <= initial);
    }
  }

  GlicKeyedService* glic_service() {
    return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->GetProfile());
  }

  GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

  base::TimeTicks animation_creation_time() { return animation_creation_time_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  const base::TimeTicks animation_creation_time_ = base::TimeTicks::Now();
};
}  // namespace

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, ExpandsWidgetSize) {
  window_controller().Toggle(nullptr, /*prevent_close=*/false,
                             InvocationSource::kOsButton);
  ASSERT_TRUE(window_controller().GetGlicWidget());

  gfx::Rect test_initial_bounds =
      window_controller().GetGlicWidget()->GetWindowBoundsInScreen();
  gfx::Rect test_new_bounds;
  test_new_bounds.set_origin(test_initial_bounds.origin());
  test_new_bounds.set_size(gfx::Size(400, 800));
  auto animation = std::make_unique<GlicWindowResizeAnimation>(
      window_controller().GetWeakPtr().get(), test_new_bounds,
      base::Milliseconds(kTestAnimationDuration), base::DoNothing());

  auto animation_api = std::make_unique<gfx::AnimationTestApi>(animation.get());
  animation_api->SetStartTime(animation_creation_time());
  animation_api->Step(animation_creation_time() +
                      base::Milliseconds(kTestAnimationDuration - 100));
  ExpectRectBetween(
      window_controller().GetGlicWidget()->GetWindowBoundsInScreen(),
      test_initial_bounds, test_new_bounds);

  animation_api->Step(animation_creation_time() +
                      base::Milliseconds(kTestAnimationDuration));
  EXPECT_EQ(test_new_bounds,
            window_controller().GetGlicWidget()->GetWindowBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, ShrinksWidgetSize) {
  window_controller().Toggle(nullptr, /*prevent_close=*/false,
                             InvocationSource::kOsButton);
  ASSERT_TRUE(window_controller().GetGlicWidget());

  gfx::Rect test_initial_bounds =
      window_controller().GetGlicWidget()->GetWindowBoundsInScreen();
  gfx::Rect test_new_bounds;
  test_new_bounds.set_origin(test_initial_bounds.origin());
  test_new_bounds.set_size(gfx::Size(1, 1));
  auto animation = std::make_unique<GlicWindowResizeAnimation>(
      window_controller().GetWeakPtr().get(), test_new_bounds,
      base::Milliseconds(kTestAnimationDuration), base::DoNothing());

  auto animation_api = std::make_unique<gfx::AnimationTestApi>(animation.get());
  animation_api->SetStartTime(animation_creation_time());
  animation_api->Step(animation_creation_time() +
                      base::Milliseconds(kTestAnimationDuration - 100));
  ExpectRectBetween(
      window_controller().GetGlicWidget()->GetWindowBoundsInScreen(),
      test_initial_bounds, test_new_bounds);

  animation_api->Step(animation_creation_time() +
                      base::Milliseconds(kTestAnimationDuration));
  EXPECT_EQ(test_new_bounds,
            window_controller().GetGlicWidget()->GetWindowBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest,
                       MovesAndChangesWidgetSize) {
  window_controller().Toggle(browser(), /*prevent_close=*/false,
                             InvocationSource::kOsButton);
  ASSERT_TRUE(window_controller().GetGlicWidget());

  gfx::Rect test_initial_bounds =
      window_controller().GetGlicWidget()->GetWindowBoundsInScreen();
  gfx::Rect test_new_bounds;

  test_new_bounds.set_origin(
      gfx::Point(test_initial_bounds.x() + 10, test_initial_bounds.y() + 10));
  test_new_bounds.set_size(gfx::Size(1, 80));
  auto animation = std::make_unique<GlicWindowResizeAnimation>(
      window_controller().GetWeakPtr().get(), test_new_bounds,
      base::Milliseconds(kTestAnimationDuration), base::DoNothing());

  auto animation_api = std::make_unique<gfx::AnimationTestApi>(animation.get());
  animation_api->SetStartTime(animation_creation_time());
  animation_api->Step(animation_creation_time() +
                      base::Milliseconds(kTestAnimationDuration - 100));
  ExpectRectBetween(
      window_controller().GetGlicWidget()->GetWindowBoundsInScreen(),
      test_initial_bounds, test_new_bounds);

  animation_api->Step(animation_creation_time() +
                      base::Milliseconds(kTestAnimationDuration));
  EXPECT_EQ(test_new_bounds,
            window_controller().GetGlicWidget()->GetWindowBoundsInScreen());
}

}  // namespace glic
