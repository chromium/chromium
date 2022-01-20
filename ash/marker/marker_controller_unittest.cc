// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/marker/marker_controller.h"

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/marker/marker_controller_test_api.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class TestMarkerObserver : public MarkerObserver {
 public:
  TestMarkerObserver() = default;
  ~TestMarkerObserver() override = default;

  // MarkerObserver:
  void OnMarkerStateChanged(bool enabled) override {
    marker_enabled_ = enabled;
  }

  bool marker_enabled() const { return marker_enabled_; }

 private:
  bool marker_enabled_ = false;
};

class MarkerControllerTest : public AshTestBase {
 public:
  MarkerControllerTest() = default;
  MarkerControllerTest(const MarkerControllerTest&) = delete;
  MarkerControllerTest& operator=(const MarkerControllerTest&) = delete;
  ~MarkerControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kProjector,
                              features::kProjectorManagedUser},
        /*disabled_features=*/{});
    AshTestBase::SetUp();
    observer_ = std::make_unique<TestMarkerObserver>();
    controller_ = MarkerController::Get();
    controller_test_api_ =
        std::make_unique<MarkerControllerTestApi>(controller_);
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    controller_test_api_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void VerifyMarkerRendererTouchEvent() {
    ui::test::EventGenerator* event_generator = GetEventGenerator();

    // When disabled the marker pointer should not be showing.
    event_generator->MoveTouch(gfx::Point(1, 1));
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());

    // Verify that by enabling the mode, the marker pointer should still not
    // be showing.
    controller_->SetEnabled(true);
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());

    // Verify that moving the stylus or finger 4 times will not display the
    // marker pointer.
    event_generator->MoveTouch(gfx::Point(2, 2));
    event_generator->MoveTouch(gfx::Point(3, 3));
    event_generator->MoveTouch(gfx::Point(4, 4));
    event_generator->MoveTouch(gfx::Point(5, 5));
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());

    // Verify that pressing the stylus or finger will show the marker pointer
    // and add a point.
    event_generator->PressTouch();
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());
    EXPECT_EQ(1, controller_test_api_->points().GetNumberOfPoints());

    // Verify that dragging the stylus or finger 2 times will add 2 more points.
    event_generator->MoveTouch(gfx::Point(6, 6));
    event_generator->MoveTouch(gfx::Point(7, 7));
    EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());

    // Verify that releasing the stylus or finger still shows the marker pointer
    // and marks gap after the last point.
    event_generator->ReleaseTouch();
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());
    EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());
    EXPECT_TRUE(controller_test_api_->points().GetNewest().gap_after);

    // Verify that disabling the mode does not hide the marker pointer.
    controller_->SetEnabled(false);
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());

    // Verify that the marker pointer does not add points while disabled.
    event_generator->PressTouch();
    event_generator->MoveTouch(gfx::Point(8, 8));
    event_generator->ReleaseTouch();
    event_generator->MoveTouch(gfx::Point(9, 9));
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());
    EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());

    // Verify that the marker pointer is not showing after clearing.
    controller_->Clear();
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());
  }

  void VerifyMarkerRendererMouseEvent() {
    ui::test::EventGenerator* event_generator = GetEventGenerator();

    // When disabled the marker pointer should not be showing.
    event_generator->MoveMouseTo(gfx::Point(1, 1));
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());

    // Verify that by enabling the mode, the marker pointer should still not
    // be showing.
    controller_->SetEnabled(true);
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());

    // Verify that moving the mouse 4 times will not show the marker pointer.
    event_generator->MoveMouseTo(gfx::Point(2, 2));
    event_generator->MoveMouseTo(gfx::Point(3, 3));
    event_generator->MoveMouseTo(gfx::Point(4, 4));
    event_generator->MoveMouseTo(gfx::Point(5, 5));
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());

    // Verify that pressing the mouse left button will show the marker pointer
    // and add a point.
    event_generator->PressLeftButton();
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());
    EXPECT_EQ(1, controller_test_api_->points().GetNumberOfPoints());

    // Verify that dragging the mouse 2 times will add 2 more points.
    event_generator->MoveMouseTo(gfx::Point(6, 6));
    event_generator->MoveMouseTo(gfx::Point(7, 7));
    EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());

    // Verify that releasing the mouse left button still shows the marker
    // pointer and marks gap after the last point.
    event_generator->ReleaseLeftButton();
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());
    EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());
    EXPECT_TRUE(controller_test_api_->points().GetNewest().gap_after);

    // Verify that disabling the mode does not hide the marker pointer.
    controller_->SetEnabled(false);
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());

    // Verify that the marker pointer does not add points while disabled.
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(gfx::Point(8, 8));
    event_generator->ReleaseLeftButton();
    event_generator->MoveMouseTo(gfx::Point(9, 9));
    EXPECT_TRUE(controller_test_api_->IsShowingMarker());
    EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());

    // Verify that the marker view is not showing after clearing.
    controller_->Clear();
    EXPECT_FALSE(controller_test_api_->IsShowingMarker());
  }

  TestMarkerObserver* observer() { return observer_.get(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  MarkerController* controller_;
  std::unique_ptr<MarkerControllerTestApi> controller_test_api_;
  std::unique_ptr<TestMarkerObserver> observer_;
};

}  // namespace

// Test to ensure the class responsible for drawing the marker pointer
// receives points from stylus movements as expected.
TEST_F(MarkerControllerTest, MarkerRendererWithStylus) {
  ash::stylus_utils::SetHasStylusInputForTesting();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  VerifyMarkerRendererTouchEvent();

  // Verify that the marker pointer does not get shown if points are not
  // coming from the stylus, even when enabled.
  event_generator->ExitPenPointerMode();
  controller_->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(10, 10));
  event_generator->MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_->IsShowingMarker());
  event_generator->ReleaseTouch();
}

// Test to ensure the class responsible for drawing the marker pointer
// receives points from finger touch movements as expected.
TEST_F(MarkerControllerTest, MarkerRendererWithTouch) {
  stylus_utils::SetNoStylusInputForTesting();
  VerifyMarkerRendererTouchEvent();
}

// Test to ensure the class responsible for drawing the marker pointer receives
// points from mouse movements as expected when stylus input is not available.
TEST_F(MarkerControllerTest, MarkerRendererMouseEventNoStylus) {
  stylus_utils::SetNoStylusInputForTesting();
  VerifyMarkerRendererMouseEvent();
}

// Test to ensure the class responsible for drawing the marker pointer receives
// points from mouse movements as expected when stylus input is available but
// hasn't been seen before.
TEST_F(MarkerControllerTest, MarkerRendererMouseEventHasStylus) {
  stylus_utils::SetHasStylusInputForTesting();

  VerifyMarkerRendererMouseEvent();

  // Verify that the marker pointer does not get shown if points are coming from
  // mouse event if a stylus interaction has been seen.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  event_generator->PressTouch();
  event_generator->MoveMouseTo(gfx::Point(2, 2));
  event_generator->MoveMouseTo(gfx::Point(3, 3));
  event_generator->MoveMouseTo(gfx::Point(4, 4));
  event_generator->MoveMouseTo(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_->IsShowingMarker());
}

// Test that observers get notified when marker activation state changed.
TEST_F(MarkerControllerTest, NotifyMarkerStateChanged) {
  controller_->AddObserver(observer());
  controller_->SetEnabled(true);
  EXPECT_TRUE(observer()->marker_enabled());
  controller_->SetEnabled(false);
  EXPECT_FALSE(observer()->marker_enabled());
  controller_->RemoveObserver(observer());
}

}  // namespace ash