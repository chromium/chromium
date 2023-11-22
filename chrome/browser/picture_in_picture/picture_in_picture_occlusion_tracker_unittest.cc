// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::_;

class MockPictureInPictureOcclusionObserver
    : public PictureInPictureOcclusionObserver {
 public:
  MockPictureInPictureOcclusionObserver() = default;
  MockPictureInPictureOcclusionObserver(
      const MockPictureInPictureOcclusionObserver&) = delete;
  MockPictureInPictureOcclusionObserver& operator=(
      const MockPictureInPictureOcclusionObserver&) = delete;
  ~MockPictureInPictureOcclusionObserver() override = default;

  // PictureInPictureOcclusionObserver:
  MOCK_METHOD(void, OnOcclusionStateChanged, (bool occluded), (override));
};

class PictureInPictureOcclusionTrackerTest : public ChromeViewsTestBase {
 public:
  PictureInPictureOcclusionTrackerTest() = default;
  PictureInPictureOcclusionTrackerTest(
      const PictureInPictureOcclusionTrackerTest&) = delete;
  PictureInPictureOcclusionTrackerTest& operator=(
      const PictureInPictureOcclusionTrackerTest&) = delete;
  ~PictureInPictureOcclusionTrackerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        media::kPictureInPictureOcclusionTracking);
    ChromeViewsTestBase::SetUp();
  }

 protected:
  std::unique_ptr<views::Widget> CreatePictureInPictureWidget() {
    // Create a picture-in-picture widget and inform the occlusion tracker.
    std::unique_ptr<views::Widget> picture_in_picture_widget =
        CreateTestWidget();
    picture_in_picture_widget->Show();
    picture_in_picture_widget->SetBounds({0, 0, 200, 200});
    PictureInPictureWindowManager::GetInstance()
        ->GetOcclusionTracker()
        ->OnPictureInPictureWidgetOpened(picture_in_picture_widget.get());
    return picture_in_picture_widget;
  }

  void WaitForBoundsChangedDebounce() {
    task_environment()->FastForwardBy(base::Milliseconds(300));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PictureInPictureOcclusionTrackerTest,
       NotifiesObserversWhenEitherWidgetMoves) {
  MockPictureInPictureOcclusionObserver observer;
  ScopedPictureInPictureOcclusionObservation observation(&observer);
  std::unique_ptr<views::Widget> picture_in_picture_widget =
      CreatePictureInPictureWidget();

  // Create a widget to track the occlusion state of, placing it so that it
  // starts out unoccluded.
  std::unique_ptr<views::Widget> occludable_widget = CreateTestWidget();
  occludable_widget->Show();
  occludable_widget->SetBounds({300, 0, 200, 200});

  // Start observing occlusion state. This should immediately tell the observer
  // that it's not occluded.
  EXPECT_CALL(observer, OnOcclusionStateChanged(false));
  observation.Observe(occludable_widget.get());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Move the occludable widget under the picture-in-picture window. This should
  // update the observer.
  EXPECT_CALL(observer, OnOcclusionStateChanged(true));
  occludable_widget->SetBounds({50, 50, 200, 200});
  WaitForBoundsChangedDebounce();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Move the picture-in-picture widget off of the occludable widget. This
  // should update the observer.
  EXPECT_CALL(observer, OnOcclusionStateChanged(false));
  picture_in_picture_widget->SetBounds({300, 0, 200, 200});
  WaitForBoundsChangedDebounce();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(PictureInPictureOcclusionTrackerTest,
       InvisiblePictureInPictureWidgetsDontOcclude) {
  MockPictureInPictureOcclusionObserver observer;
  ScopedPictureInPictureOcclusionObservation observation(&observer);
  std::unique_ptr<views::Widget> picture_in_picture_widget =
      CreatePictureInPictureWidget();

  // Create a widget to track the occlusion state of, placing it so that it
  // starts out occluded.
  std::unique_ptr<views::Widget> occludable_widget = CreateTestWidget();
  occludable_widget->Show();
  occludable_widget->SetBounds({50, 50, 200, 200});

  // Start observing occlusion state. This should immediately tell the observer
  // that it's occluded.
  EXPECT_CALL(observer, OnOcclusionStateChanged(true));
  observation.Observe(occludable_widget.get());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Make the picture-in-picture widget invisible. This should inform the
  // observer that it's no longer occluded.
  EXPECT_CALL(observer, OnOcclusionStateChanged(false));
  picture_in_picture_widget->Hide();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Show the picture-in-picture widget again. This should inform the observer
  // that it's occluded again.
  EXPECT_CALL(observer, OnOcclusionStateChanged(true));
  picture_in_picture_widget->Show();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(PictureInPictureOcclusionTrackerTest,
       UpdatesOcclusionStateWhenWidgetMovesDueToParentMoving) {
  MockPictureInPictureOcclusionObserver observer;
  ScopedPictureInPictureOcclusionObservation observation(&observer);
  std::unique_ptr<views::Widget> picture_in_picture_widget =
      CreatePictureInPictureWidget();

  // Create a parent widget for our occludable widget that is underneath the
  // picture-in-picture window.
  std::unique_ptr<views::Widget> parent_widget = CreateTestWidget();
  parent_widget->Show();
  parent_widget->SetBounds({0, 0, 200, 200});

  // Create an occludable widget that is a child of `parent_widget`.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = parent_widget->GetNativeView();
  params.child = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 50, 50);
  std::unique_ptr<views::Widget> occludable_widget =
      CreateTestWidget(std::move(params));
  occludable_widget->Show();

  // Start observing `occludable_widget`. This should immediately inform the
  // observer that the window is occluded.
  EXPECT_CALL(observer, OnOcclusionStateChanged(true));
  observation.Observe(occludable_widget.get());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Indirectly move `occludable_widget` out from underneath the
  // picture-in-picture window by moving its parent window. This should inform
  // the observer that the window is no longer occluded.
  EXPECT_CALL(observer, OnOcclusionStateChanged(false));
  parent_widget->SetBounds({300, 0, 200, 200});
  WaitForBoundsChangedDebounce();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(PictureInPictureOcclusionTrackerTest, MultipleObserversForOneWidget) {
  MockPictureInPictureOcclusionObserver observer1;
  ScopedPictureInPictureOcclusionObservation observation1(&observer1);
  std::unique_ptr<views::Widget> picture_in_picture_widget =
      CreatePictureInPictureWidget();

  // Create a widget to track the occlusion state of, placing it so that it
  // starts out occluded.
  std::unique_ptr<views::Widget> occludable_widget = CreateTestWidget();
  occludable_widget->Show();
  occludable_widget->SetBounds({50, 50, 200, 200});

  // Start observing `occludable_widget`. This should immediately inform the
  // observer that the window is occluded.
  EXPECT_CALL(observer1, OnOcclusionStateChanged(true));
  observation1.Observe(occludable_widget.get());
  testing::Mock::VerifyAndClearExpectations(&observer1);

  {
    MockPictureInPictureOcclusionObserver observer2;
    ScopedPictureInPictureOcclusionObservation observation2(&observer2);

    // Start observing `occludable_widget` with a second observer. This should
    // immediately inform the new observer but not the original one.
    EXPECT_CALL(observer1, OnOcclusionStateChanged(_)).Times(0);
    EXPECT_CALL(observer2, OnOcclusionStateChanged(true));
    observation2.Observe(occludable_widget.get());
    testing::Mock::VerifyAndClearExpectations(&observer1);
    testing::Mock::VerifyAndClearExpectations(&observer2);

    // Unocclude the widget. This should inform both observers.
    EXPECT_CALL(observer1, OnOcclusionStateChanged(false));
    EXPECT_CALL(observer2, OnOcclusionStateChanged(false));
    picture_in_picture_widget->SetBounds({300, 0, 200, 200});
    WaitForBoundsChangedDebounce();
    testing::Mock::VerifyAndClearExpectations(&observer1);
    testing::Mock::VerifyAndClearExpectations(&observer2);
  }

  // The second observer is now out of scope, but the original observer should
  // still receive updates.
  EXPECT_CALL(observer1, OnOcclusionStateChanged(true));
  picture_in_picture_widget->SetBounds({0, 0, 200, 200});
  WaitForBoundsChangedDebounce();
  testing::Mock::VerifyAndClearExpectations(&observer1);
}

TEST_F(PictureInPictureOcclusionTrackerTest,
       ChildOfPictureInPictureWidgetIsntOccluded) {
  MockPictureInPictureOcclusionObserver observer;
  ScopedPictureInPictureOcclusionObservation observation(&observer);
  std::unique_ptr<views::Widget> picture_in_picture_widget =
      CreatePictureInPictureWidget();

  // Create an occludable widget that is a child of `picture_in_picture_widget`.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = picture_in_picture_widget->GetNativeView();
  params.child = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 50, 50);
  std::unique_ptr<views::Widget> occludable_widget =
      CreateTestWidget(std::move(params));
  occludable_widget->Show();

  // Start observing occlusion state. Even though the picture-in-picture widget
  // intersects with the widget, it should not be considered occluded since it
  // is a child of the picture-in-picture widget.
  EXPECT_CALL(observer, OnOcclusionStateChanged(false));
  observation.Observe(occludable_widget.get());
  testing::Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace
