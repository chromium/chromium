// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/scoped_nudge_pause.h"
#include "ash/public/cpp/system/system_nudge_pause_manager.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/system/tray/system_nudge_controller.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kNudgeMargin = 8;
constexpr int kIconSize = 20;
constexpr int kIconLabelSpacing = 16;
constexpr int kNudgePadding = 16;
constexpr int kNudgeWidth = 120;

constexpr char kNudgeName[] = "TestSystemNudge";

constexpr NudgeCatalogName kTestCatalogName =
    NudgeCatalogName::kTestCatalogName;

constexpr char kNudgeShownCount[] = "Ash.NotifierFramework.Nudge.ShownCount";
constexpr char kNudgeTimeToActionWithin1m[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
constexpr char kNudgeTimeToActionWithin1h[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";
constexpr char kNudgeTimeToActionWithinSession[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.WithinSession";

constexpr base::TimeDelta kNudgeFadeAnimationTime = base::Milliseconds(250);
constexpr base::TimeDelta kNudgeShowTime = base::Seconds(10);

gfx::VectorIcon kEmptyIcon;

class TestSystemNudge : public SystemNudge {
 public:
  explicit TestSystemNudge(NudgeCatalogName catalog_name = kTestCatalogName)
      : SystemNudge(kNudgeName,
                    catalog_name,
                    kIconSize,
                    kIconLabelSpacing,
                    kNudgePadding) {}

  gfx::Rect GetWidgetBounds() {
    return widget()->GetClientAreaBoundsInScreen();
  }

 private:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override {
    return std::make_unique<SystemNudgeLabel>(std::u16string(), kNudgeWidth);
  }

  const gfx::VectorIcon& GetIcon() const override { return kEmptyIcon; }

  std::u16string GetAccessibilityText() const override {
    return std::u16string();
  }
};

class TestSystemNudgeController : public SystemNudgeController {
 public:
  TestSystemNudgeController() = default;
  TestSystemNudgeController(const TestSystemNudgeController&) = delete;
  TestSystemNudgeController& operator=(const TestSystemNudgeController&) =
      delete;
  ~TestSystemNudgeController() override = default;

  // SystemNudgeController:
  std::unique_ptr<SystemNudge> CreateSystemNudge() override {
    return std::make_unique<TestSystemNudge>(kTestCatalogName);
  }
};

}  // namespace

class SystemNudgeTest : public AshTestBase {
 public:
  SystemNudgeTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  SystemNudgeTest(const SystemNudgeTest&) = delete;
  SystemNudgeTest& operator=(const SystemNudgeTest&) = delete;
  ~SystemNudgeTest() override = default;

  void SetState(ShelfVisibilityState visibility_state) {
    GetPrimaryShelf()->shelf_layout_manager()->SetState(visibility_state,
                                                        /*force_layout=*/false);
  }
};

TEST_F(SystemNudgeTest, NudgeDefaultOnLeftSide) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  TestSystemNudge nudge;

  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kBottomLocked);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x() + shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

TEST_F(SystemNudgeTest, ShownCountMetric) {
  base::HistogramTester histogram_tester;
  auto nudge_controller = std::make_unique<TestSystemNudgeController>();

  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 0);

  nudge_controller->ShowNudge();
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 1);

  nudge_controller->ShowNudge();
  nudge_controller->ShowNudge();
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 3);
}

TEST_F(SystemNudgeTest, TimeToActionMetric) {
  base::HistogramTester histogram_tester;
  auto nudge_controller = std::make_unique<TestSystemNudgeController>();
  nudge_controller->ResetNudgeRegistryForTesting();

  // Metric is not recorded if nudge has not been shown.
  SystemNudgeController::MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 0);

  // Metric is recorded after nudge is shown.
  nudge_controller->ShowNudge();
  task_environment()->FastForwardBy(base::Seconds(1));
  SystemNudgeController::MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  SystemNudgeController::MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is recorded with appropriate time range after showing nudge again
  // and waiting the time to fall into the next time bucket.
  nudge_controller->ShowNudge();
  task_environment()->FastForwardBy(base::Minutes(2));
  SystemNudgeController::MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  nudge_controller->ShowNudge();
  task_environment()->FastForwardBy(base::Hours(2));
  SystemNudgeController::MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);
}

TEST_F(SystemNudgeTest, NudgePositionChangeWhenShelfAutoHide) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;
  TestSystemNudge nudge;

  // Enables the auto hide behavior.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Tests when the shelf is on the bottom.
  shelf->SetAlignment(ShelfAlignment::kBottom);

  // When the shelf is visiblie, the bottom of the nudge should be on the top of
  // the the shelf (with `-self_size`).
  SetState(ShelfVisibilityState::SHELF_VISIBLE);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // When the shelf is hidden, the bottom of the nudge should be just on the
  // display bottom.
  SetState(ShelfVisibilityState::SHELF_HIDDEN);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

TEST_F(SystemNudgeTest, NudgePositionWithBottomLocked) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;
  TestSystemNudge nudge;

  // Enables the auto hide behavior.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Tests when the shelf is on the bottom with locked state.
  shelf->SetAlignment(ShelfAlignment::kBottomLocked);

  // When the shelf is visiblie, the bottom of the nudge should be on the top of
  // the the shelf (with `-self_size`).
  SetState(ShelfVisibilityState::SHELF_VISIBLE);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // When the shelf is hidden, the bottom of the nudge should be just on the
  // display bottom.
  SetState(ShelfVisibilityState::SHELF_HIDDEN);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

TEST_F(SystemNudgeTest, DismissTimer) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto nudge_controller = std::make_unique<TestSystemNudgeController>();

  // Show a nudge.
  EXPECT_FALSE(nudge_controller->GetSystemNudgeForTesting());
  nudge_controller->ShowNudge();
  auto* nudge = nudge_controller->GetSystemNudgeForTesting();
  ASSERT_TRUE(nudge);

  // Attempt hiding the nudge while it's animating in, the hide request should
  // be ignored.
  EXPECT_TRUE(nudge->widget()->GetLayer()->GetAnimator()->is_animating());
  nudge_controller->HideNudge();
  EXPECT_TRUE(nudge->widget());

  // Fast forward the animation time, the nudge should have finished animating.
  task_environment()->FastForwardBy(kNudgeFadeAnimationTime +
                                    (kNudgeShowTime / 2));
  EXPECT_FALSE(nudge->widget()->GetLayer()->GetAnimator()->is_animating());

  // Fast forward nudge's default duration, the nudge should have been
  // dismissed.
  task_environment()->FastForwardBy(kNudgeShowTime + (kNudgeShowTime / 2));
  EXPECT_FALSE(nudge_controller->GetSystemNudgeForTesting());
}

TEST_F(SystemNudgeTest, CloseNudgeImmediately) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto nudge_controller = std::make_unique<TestSystemNudgeController>();

  // Show a nudge.
  EXPECT_FALSE(nudge_controller->GetSystemNudgeForTesting());
  nudge_controller->ShowNudge();
  auto* nudge = nudge_controller->GetSystemNudgeForTesting();
  ASSERT_TRUE(nudge);

  // Call `CloseNudge()` to close a nudge immediately.
  nudge_controller->CloseNudge();
  EXPECT_FALSE(nudge_controller->GetSystemNudgeForTesting());
}

TEST_F(SystemNudgeTest, ShowNudgeWithScopedNudgePause) {
  auto nudge_controller = std::make_unique<TestSystemNudgeController>();
  auto scoped_nudge_pause = SystemNudgePauseManager::Get()->CreateScopedPause();
  ASSERT_FALSE(nudge_controller->GetSystemNudgeForTesting());

  // When a `ScopedNudgePause` is present, no nudge will be shown.
  nudge_controller->ShowNudge();
  ASSERT_FALSE(nudge_controller->GetSystemNudgeForTesting());

  // Destroy the `ScopedNudgePause`, the nudge doesn't exist either.
  scoped_nudge_pause.reset();
  ASSERT_FALSE(nudge_controller->GetSystemNudgeForTesting());
}

TEST_F(SystemNudgeTest, CancelNudgeWithScopedNudgePause) {
  auto nudge_controller = std::make_unique<TestSystemNudgeController>();
  ASSERT_FALSE(nudge_controller->GetSystemNudgeForTesting());

  // Firstly, the nudge will be shown.
  nudge_controller->ShowNudge();
  ASSERT_TRUE(nudge_controller->GetSystemNudgeForTesting());

  // After a `ScopedNudgePause` is created, the nudge will be closed
  // immediately.
  SystemNudgePauseManager::Get()->CreateScopedPause();
  ASSERT_FALSE(nudge_controller->GetSystemNudgeForTesting());
}

}  // namespace ash
