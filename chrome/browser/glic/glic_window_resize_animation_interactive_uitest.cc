// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/glic_window_resize_animation.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace glic {
namespace {

constexpr base::TimeDelta kTestAnimationDuration = base::Milliseconds(300);

void Append(std::string* a, const char* b) {
  *a += b;
}

base::OnceClosure BindAppend(std::string* a, const char* b) {
  return base::BindOnce(&Append, a, b);
}

class GlicWindowResizeAnimationTest : public test::InteractiveGlicTest {
 public:
  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();

    RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached));
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

  std::pair<std::unique_ptr<GlicWindowResizeAnimation>,
            std::unique_ptr<gfx::AnimationTestApi>>
  MakeAnimation(const gfx::Rect& target_bounds,
                base::TimeDelta duration,
                base::OnceClosure callback) {
    auto animation = std::make_unique<GlicWindowResizeAnimation>(
        &window_controller(), window_controller().window_animator(),
        target_bounds, duration, std::move(callback));
    auto test_api = std::make_unique<gfx::AnimationTestApi>(animation.get());
    test_api->SetStartTime(animation_creation_time_);
    return {std::move(animation), std::move(test_api)};
  }

  gfx::Rect GetWidgetBounds() {
    return window_controller().GetGlicWidget()->GetWindowBoundsInScreen();
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

  base::TimeTicks animation_creation_time() { return animation_creation_time_; }

 private:
  const base::TimeTicks animation_creation_time_ = base::TimeTicks::Now();
};
}  // namespace

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, ExpandsWidgetSize) {
  gfx::Rect test_initial_bounds = GetWidgetBounds();
  gfx::Rect test_new_bounds;
  test_new_bounds.set_origin(test_initial_bounds.origin());
  test_new_bounds.set_size(gfx::Size(400, 800));

  auto [animation, test_api] =
      MakeAnimation(test_new_bounds, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), test_initial_bounds, test_new_bounds);

  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(test_new_bounds, GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, ShrinksWidgetSize) {
  gfx::Rect test_initial_bounds = GetWidgetBounds();
  gfx::Rect test_new_bounds(test_initial_bounds.origin(), gfx::Size(1, 1));

  auto [animation, test_api] =
      MakeAnimation(test_new_bounds, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), test_initial_bounds, test_new_bounds);

  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(test_new_bounds, GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest,
                       MovesAndChangesWidgetSize) {
  gfx::Rect test_initial_bounds = GetWidgetBounds();
  gfx::Rect test_new_bounds(
      gfx::Point(test_initial_bounds.x() + 10, test_initial_bounds.y() + 10),
      gfx::Size(1, 80));

  auto [animation, test_api] =
      MakeAnimation(test_new_bounds, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), test_initial_bounds, test_new_bounds);

  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(test_new_bounds, GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, UpdateTargetPosition) {
  gfx::Rect initial_bounds = GetWidgetBounds();
  gfx::Rect target_bounds_1(10, 10, 20, 20);

  // Start changing size and position
  auto [animation, test_api] =
      MakeAnimation(target_bounds_1, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), initial_bounds, target_bounds_1);

  // Update position, advance to the end
  gfx::Point new_target_point(50, 50);
  gfx::Rect new_target(new_target_point, target_bounds_1.size());
  animation->UpdateTargetBounds(new_target, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration);

  // Widget should now have the latest target origin and size.
  EXPECT_EQ(gfx::Rect(new_target_point, target_bounds_1.size()),
            GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, UpdateTargetSize) {
  gfx::Rect initial_bounds = GetWidgetBounds();
  gfx::Rect target_bounds_1(100, 100, 20, 20);

  // Start changing size and position
  auto [animation, test_api] =
      MakeAnimation(target_bounds_1, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), initial_bounds, target_bounds_1);

  // Update size, advance to the end
  gfx::Size new_target_size(50, 50);
  gfx::Rect new_target(target_bounds_1.origin(), new_target_size);
  animation->UpdateTargetBounds(new_target, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration);

  // Widget should now have the latest target origin and size.
  EXPECT_EQ(gfx::Rect(target_bounds_1.origin(), new_target_size),
            GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, AllCallbacksRunInOrder) {
  gfx::Rect initial_bounds = GetWidgetBounds();
  gfx::Rect target_bounds_1(10, 10, 20, 20);

  std::string call_log;

  // Start animating
  auto [animation, test_api] = MakeAnimation(
      target_bounds_1, kTestAnimationDuration, BindAppend(&call_log, "1"));
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), initial_bounds, target_bounds_1);

  // Make some updates
  gfx::Rect target_bounds_2(target_bounds_1.origin(), gfx::Size(50, 50));
  animation->UpdateTargetBounds(target_bounds_2, BindAppend(&call_log, " 2"));
  gfx::Rect target_bounds_3(target_bounds_2.origin(), gfx::Size(60, 60));
  animation->UpdateTargetBounds(target_bounds_3, BindAppend(&call_log, " 3"));
  gfx::Rect target_bounds_4(gfx::Point(100, 100), target_bounds_3.size());
  animation->UpdateTargetBounds(target_bounds_4, BindAppend(&call_log, " 4"));

  // Advance to the end. Widget should be at its final bounds.
  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(gfx::Rect(100, 100, 60, 60), GetWidgetBounds());

  // Callbacks should run in the order in which they were added.
  test_api.reset();
  animation.reset();
  // TODO: Find another way to wait for the tasks to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("1 2 3 4", call_log);
}

}  // namespace glic
