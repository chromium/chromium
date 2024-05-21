// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_transform_window.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/numerics/safe_conversions.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

float GetItemScale(int source_height,
                   int target_height,
                   int top_view_inset,
                   int title_height) {
  return ScopedOverviewTransformWindow::GetItemScale(
      source_height, target_height, top_view_inset, title_height);
}

}  // namespace

using ScopedOverviewTransformWindowTest = AshTestBase;

// Tests that transformed Rect scaling preserves its aspect ratio. The window
// scale is determined by the target height and so the test is actually testing
// that the width is calculated correctly. Since all calculations are done with
// floating point values and then safely converted to integers (using ceiled and
// floored values where appropriate), the  expectations are forgiving (use
// *_NEAR) within a single pixel.
TEST_F(ScopedOverviewTransformWindowTest, TransformedRectMaintainsAspect) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(10, 10, 100, 100));
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());

  gfx::RectF rect(50.f, 50.f, 200.f, 400.f);
  gfx::RectF bounds(100.f, 100.f, 50.f, 50.f);
  gfx::RectF transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  float scale = GetItemScale(rect.height(), bounds.height(), 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::RectF(50.f, 50.f, 400.f, 200.f);
  scale = GetItemScale(rect.height(), bounds.height(), 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::RectF(50.f, 50.f, 25.f, 25.f);
  scale = GetItemScale(rect.height(), bounds.height(), 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::RectF(50.f, 50.f, 25.f, 50.f);
  scale = GetItemScale(rect.height(), bounds.height(), 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::RectF(50.f, 50.f, 50.f, 25.f);
  scale = GetItemScale(rect.height(), bounds.height(), 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered.
TEST_F(ScopedOverviewTransformWindowTest, TransformedRectIsCentered) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(10, 10, 100, 100));
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  gfx::RectF rect(50.f, 50.f, 200.f, 400.f);
  gfx::RectF bounds(100.f, 100.f, 50.f, 50.f);
  gfx::RectF transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_GE(transformed_rect.x(), bounds.x());
  EXPECT_LE(transformed_rect.right(), bounds.right());
  EXPECT_GE(transformed_rect.y(), bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(transformed_rect.y() - bounds.y(),
              bounds.bottom() - transformed_rect.bottom(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered
// when inset and header height are specified.
TEST_F(ScopedOverviewTransformWindowTest, TransformedRectIsCenteredWithInset) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(10, 10, 100, 100));
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  gfx::RectF rect(50.f, 50.f, 400.f, 200.f);
  gfx::RectF bounds(100.f, 100.f, 50.f, 50.f);
  const int inset = 20;
  const int header_height = 10;
  const float scale =
      GetItemScale(rect.height(), bounds.height(), inset, header_height);
  gfx::RectF transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, inset,
                                                            header_height);
  // The |rect| width does not fit and therefore it gets centered outside
  // |bounds| starting before |bounds.x()| and ending after |bounds.right()|.
  EXPECT_LE(transformed_rect.x(), bounds.x());
  EXPECT_GE(transformed_rect.right(), bounds.right());
  EXPECT_GE(
      transformed_rect.y() + base::ClampCeil(scale * inset) - header_height,
      bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(
      transformed_rect.y() + (int)(scale * inset) - header_height - bounds.y(),
      bounds.bottom() - transformed_rect.bottom(), 1);
}

// Verify that a window which will be displayed like a letter box on the window
// grid has the correct bounds.
TEST_F(ScopedOverviewTransformWindowTest, TransformingLetteredRect) {
  // Create a window whose width is more than twice the height.
  const gfx::Rect original_bounds(10, 10, 300, 100);
  const int scale = 3;
  std::unique_ptr<aura::Window> window = CreateTestWindow(original_bounds);
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  EXPECT_EQ(OverviewItemFillMode::kLetterBoxed, transform_window.fill_mode());

  // Without any headers, the width should match the target, and the height
  // should be such that the aspect ratio of |original_bounds| is maintained.
  const gfx::RectF overview_bounds(100.f, 100.f);
  gfx::RectF transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(
          gfx::RectF(original_bounds), overview_bounds, 0, 0);
  EXPECT_EQ(overview_bounds.width(), transformed_rect.width());
  EXPECT_NEAR(overview_bounds.height() / scale, transformed_rect.height(), 1);

  // With headers, the width should still match the target. The height should
  // still be such that the aspect ratio is maintained, but the original header
  // which is hidden in overview needs to be accounted for.
  const int original_header = 10;
  const int overview_header = 20;
  transformed_rect = transform_window.ShrinkRectToFitPreservingAspectRatio(
      gfx::RectF(original_bounds), overview_bounds, original_header,
      overview_header);
  EXPECT_EQ(overview_bounds.width(), transformed_rect.width());
  EXPECT_NEAR((overview_bounds.height() - original_header) / scale,
              transformed_rect.height() - original_header / scale, 1);
  EXPECT_TRUE(overview_bounds.Contains(transformed_rect));
}

// Verify that a window which will be displayed like a pillar box on the window
// grid has the correct bounds.
TEST_F(ScopedOverviewTransformWindowTest, TransformingPillaredRect) {
  // Create a window whose height is more than twice the width.
  const gfx::Rect original_bounds(10, 10, 150, 450);
  const int scale = 3;
  std::unique_ptr<aura::Window> window = CreateTestWindow(original_bounds);
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  EXPECT_EQ(OverviewItemFillMode::kPillarBoxed, transform_window.fill_mode());

  // Without any headers, the height should match the target, and the width
  // should be such that the aspect ratio of |original_bounds| is maintained.
  const gfx::RectF overview_bounds(100.f, 100.f);
  gfx::RectF transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(
          gfx::RectF(original_bounds), overview_bounds, 0, 0);
  EXPECT_EQ(overview_bounds.height(), transformed_rect.height());
  EXPECT_NEAR(overview_bounds.width() / scale, transformed_rect.width(), 1);

  // With headers, the height should not include the area reserved for the
  // overview window title. It also needs to account for the original header
  // which will become hidden in overview mode.
  const int original_header = 10;
  const int overview_header = 20;
  transformed_rect = transform_window.ShrinkRectToFitPreservingAspectRatio(
      gfx::RectF(original_bounds), overview_bounds, original_header,
      overview_header);
  const float overview_scale =
      original_bounds.height() / overview_bounds.height();
  const float expected_height = overview_bounds.height() - overview_header +
                                original_header / overview_scale;
  EXPECT_NEAR(expected_height, transformed_rect.height(), 1);
  EXPECT_TRUE(overview_bounds.Contains(transformed_rect));
}

// Tests the cases when very wide or tall windows enter overview mode.
TEST_F(ScopedOverviewTransformWindowTest, ExtremeWindowBounds) {
  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Window |wide|, with size (400, 160) will be resized to (300, 160)
  // when the 400x300 is rotated to 300x400, and should be considered a normal
  // overview window after display change.
  UpdateDisplay("400x300");
  std::unique_ptr<aura::Window> wide = CreateTestWindow(gfx::Rect(400, 160));
  std::unique_ptr<aura::Window> tall = CreateTestWindow(gfx::Rect(100, 300));
  std::unique_ptr<aura::Window> normal = CreateTestWindow(gfx::Rect(300, 300));

  ScopedOverviewTransformWindow scoped_wide(nullptr, wide.get());
  ScopedOverviewTransformWindow scoped_tall(nullptr, tall.get());
  ScopedOverviewTransformWindow scoped_normal(nullptr, normal.get());

  // Verify the window dimension type is as expected after entering overview
  // mode.
  EXPECT_EQ(OverviewItemFillMode::kLetterBoxed, scoped_wide.fill_mode());
  EXPECT_EQ(OverviewItemFillMode::kPillarBoxed, scoped_tall.fill_mode());
  EXPECT_EQ(OverviewItemFillMode::kNormal, scoped_normal.fill_mode());

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  scoped_wide.UpdateOverviewItemFillMode();
  scoped_tall.UpdateOverviewItemFillMode();
  scoped_normal.UpdateOverviewItemFillMode();

  // Verify that |wide| has its window dimension type updated after the display
  // change.
  EXPECT_EQ(OverviewItemFillMode::kNormal, scoped_wide.fill_mode());
  EXPECT_EQ(OverviewItemFillMode::kPillarBoxed, scoped_tall.fill_mode());
  EXPECT_EQ(OverviewItemFillMode::kNormal, scoped_normal.fill_mode());
}

// Tests that transients which should be invisible in overview do not have their
// transforms or opacities altered.
TEST_F(ScopedOverviewTransformWindowTest, InvisibleTransients) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto child = CreateTestWindow(gfx::Rect(100, 190, 100, 10),
                                aura::client::WINDOW_TYPE_POPUP);
  auto child2 = CreateTestWindow(gfx::Rect(0, 190, 100, 10),
                                 aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(window.get(), child.get());
  ::wm::AddTransientChild(window.get(), child2.get());

  child2->SetProperty(kHideInOverviewKey, true);

  for (auto* it : {window.get(), child.get(), child2.get()}) {
    it->SetTransform(gfx::Transform());
    it->layer()->SetOpacity(1.f);
  }

  ScopedOverviewTransformWindow scoped_window(nullptr, window.get());
  scoped_window.SetOpacity(0.5f);
  EXPECT_EQ(0.5f, window->layer()->opacity());
  EXPECT_EQ(0.5f, child->layer()->opacity());
  EXPECT_EQ(0.f, child2->layer()->opacity());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(child->IsVisible());
  EXPECT_FALSE(child2->IsVisible());

  auto transform = gfx::Transform::MakeTranslation(10.f, 10.f);
  window_util::SetTransform(window.get(), transform);
  EXPECT_EQ(transform, window->transform());
  EXPECT_EQ(transform, child->transform());
  EXPECT_TRUE(child2->transform().IsIdentity());
}

// Tests that the transient window which should be invisible in overview is not
// visible even if the window property is changed after initializing
// ScopedOverviewTransformWindow.
TEST_F(ScopedOverviewTransformWindowTest,
       InvisibleTransientsPropertyChangeAfterInit) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto child = CreateTestWindow(gfx::Rect(100, 190, 100, 10),
                                aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(window.get(), child.get());

  ScopedOverviewTransformWindow scoped_window(nullptr, window.get());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  // Change property after construction of |scoped_window|.
  child->SetProperty(kHideInOverviewKey, true);
  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // Clear property after construction of |scoped_window|.
  child->ClearProperty(kHideInOverviewKey);
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  // Change to hide again.
  child->SetProperty(kHideInOverviewKey, true);
  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(child->IsVisible());
}

// Tests that the transient window which should be invisible in overview is not
// visible even if the window is added after initializing
// ScopedOverviewTransformWindow.
TEST_F(ScopedOverviewTransformWindowTest, InvisibleTransientsAddedAfterInit) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto child = CreateTestWindow(gfx::Rect(100, 190, 100, 10),
                                aura::client::WINDOW_TYPE_POPUP);
  auto child2 = CreateTestWindow(gfx::Rect(0, 190, 100, 10),
                                 aura::client::WINDOW_TYPE_POPUP);
  child2->SetProperty(kHideInOverviewKey, true);

  ScopedOverviewTransformWindow scoped_window(nullptr, window.get());

  // Add visible transient after construction of |scoped_window|.
  ::wm::AddTransientChild(window.get(), child.get());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  // Add invisible transient after construction of |scoped_window|.
  ::wm::AddTransientChild(window.get(), child2.get());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(child->IsVisible());
  EXPECT_FALSE(child2->IsVisible());
}

// Tests that the event targeting policies of a given window and transient
// descendants gets set as expected.
TEST_F(ScopedOverviewTransformWindowTest, EventTargetingPolicy) {
  using etp = aura::EventTargetingPolicy;

  // Helper for creating popups that will be transients for testing.
  auto create_popup = [this] {
    std::unique_ptr<aura::Window> popup =
        CreateTestWindow(gfx::Rect(10, 10), aura::client::WINDOW_TYPE_POPUP);
    popup->SetEventTargetingPolicy(etp::kTargetAndDescendants);
    return popup;
  };

  auto window = CreateTestWindow(gfx::Rect(200, 200));
  window->SetEventTargetingPolicy(etp::kTargetAndDescendants);

  auto transient = create_popup();
  auto transient1 = create_popup();
  auto transient2 = create_popup();
  ::wm::AddTransientChild(window.get(), transient.get());

  {
    // Tests that after creating the scoped object, the window and its current
    // transient child have |kNone| targeting policy.
    ScopedOverviewTransformWindow scoped_window(nullptr, window.get());
    EXPECT_EQ(etp::kNone, window->event_targeting_policy());
    EXPECT_EQ(etp::kNone, transient->event_targeting_policy());

    // Tests that after adding transient children, one to the window itself and
    // one to the current transient child, they will both have |kNone| targeting
    // policy.
    ::wm::AddTransientChild(window.get(), transient1.get());
    ::wm::AddTransientChild(transient.get(), transient2.get());
    EXPECT_EQ(etp::kNone, transient1->event_targeting_policy());
    EXPECT_EQ(etp::kNone, transient2->event_targeting_policy());

    // Tests that adding a transient child which does not have |window| as its
    // descendant does not have its targeting policy altered.
    auto window2 = CreateTestWindow(gfx::Rect(200, 200));
    auto transient3 = create_popup();
    ::wm::AddTransientChild(window2.get(), transient3.get());
    EXPECT_EQ(etp::kTargetAndDescendants, transient3->event_targeting_policy());

    // Tests that removing a transient child from |window| will reset its
    // targeting policy.
    ::wm::RemoveTransientChild(window.get(), transient1.get());
    EXPECT_EQ(etp::kTargetAndDescendants, transient1->event_targeting_policy());
  }

  // Tests that when the scoped object is destroyed, the targeting policies all
  // get reset.
  EXPECT_EQ(etp::kTargetAndDescendants, window->event_targeting_policy());
  EXPECT_EQ(etp::kTargetAndDescendants, transient->event_targeting_policy());
  EXPECT_EQ(etp::kTargetAndDescendants, transient2->event_targeting_policy());
}

}  // namespace ash
