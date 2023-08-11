// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_fling_controller.h"

#include <utility>

#include "cc/input/snap_fling_curve.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace test {
namespace {

class MockSnapFlingClient : public SnapFlingClient {
 public:
  MOCK_CONST_METHOD4(GetSnapFlingInfoAndSetAnimatingSnapTarget,
                     bool(const gfx::Vector2dF& current_delta,
                          const gfx::Vector2dF& natural_displacement,
                          gfx::PointF* initial_offset,
                          gfx::PointF* target_offset));
  MOCK_METHOD1(ScrollEndForSnapFling, void(bool));
  MOCK_METHOD0(RequestAnimationForSnapFling, void());
  MOCK_METHOD1(ScrollByForSnapFling, gfx::PointF(const gfx::Vector2dF&));
};

class MockSnapFlingCurve : public SnapFlingCurve {
 public:
  MockSnapFlingCurve()
      : SnapFlingCurve(gfx::PointF(), gfx::PointF(0, 100), base::TimeTicks()) {}
  MOCK_CONST_METHOD0(IsFinished, bool());
  MOCK_METHOD1(GetScrollDelta, gfx::Vector2dF(base::TimeTicks));
};

}  // namespace

class SnapFlingControllerTest : public testing::Test {
 public:
  SnapFlingControllerTest() {
    controller_ = std::make_unique<SnapFlingController>(&mock_client_);
  }
  void SetCurve(std::unique_ptr<SnapFlingCurve> curve) {
    controller_->SetCurveForTest(std::move(curve));
  }
  void SetActiveState() { controller_->SetActiveStateForTest(); }

 protected:
  testing::StrictMock<MockSnapFlingClient> mock_client_;
  std::unique_ptr<SnapFlingController> controller_;
};

TEST_F(SnapFlingControllerTest, DoesNotFilterGSBWhenIdle) {
  EXPECT_FALSE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kBegin));
}

TEST_F(SnapFlingControllerTest, FiltersGSUAndGSEDependingOnState) {
  // Should not filter GSU and GSE if the fling is not active.
  EXPECT_FALSE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kUpdate));
  EXPECT_FALSE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kEnd));

  // Should filter GSU and GSE if the fling is active.
  SetActiveState();
  EXPECT_TRUE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kUpdate));
  EXPECT_TRUE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kEnd));
}

TEST_F(SnapFlingControllerTest, CreatesAndAnimatesCurveOnFirstInertialGSU) {
  SnapFlingController::GestureScrollUpdateInfo gsu;
  gsu.delta = gfx::Vector2dF(0, -10);
  gsu.is_in_inertial_phase = true;

  EXPECT_CALL(mock_client_, GetSnapFlingInfoAndSetAnimatingSnapTarget(
                                testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                               testing::SetArgPointee<3>(gfx::PointF(0, 100)),
                               testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(1);
  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(gsu));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, ScrollEndWhenHasEqualOffsetsOnInertialGSU) {
  SnapFlingController::GestureScrollUpdateInfo gsu;
  gsu.delta = gfx::Vector2dF(0, -10);
  gsu.is_in_inertial_phase = true;

  EXPECT_CALL(mock_client_, GetSnapFlingInfoAndSetAnimatingSnapTarget(
                                testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                               testing::SetArgPointee<3>(gfx::PointF(0, 0)),
                               testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, ScrollEndForSnapFling(true)).Times(1);
  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(gsu));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, DoesNotHandleNonInertialGSU) {
  SnapFlingController::GestureScrollUpdateInfo gsu;
  gsu.delta = gfx::Vector2dF(0, -10);
  gsu.is_in_inertial_phase = false;

  EXPECT_CALL(mock_client_, GetSnapFlingInfoAndSetAnimatingSnapTarget(
                                testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, AnimatesTheCurve) {
  std::unique_ptr<MockSnapFlingCurve> mock_curve =
      std::make_unique<MockSnapFlingCurve>();
  MockSnapFlingCurve* curve = mock_curve.get();
  SetCurve(std::move(mock_curve));

  EXPECT_CALL(*curve, IsFinished()).WillOnce(testing::Return(false));
  EXPECT_CALL(*curve, GetScrollDelta(testing::_))
      .WillOnce(testing::Return(gfx::Vector2dF(100, 100)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(gfx::Vector2dF(100, 100)));
  controller_->Animate(base::TimeTicks() + base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
  testing::Mock::VerifyAndClearExpectations(curve);
}

TEST_F(SnapFlingControllerTest, FinishesTheCurve) {
  std::unique_ptr<MockSnapFlingCurve> mock_curve =
      std::make_unique<MockSnapFlingCurve>();
  MockSnapFlingCurve* curve = mock_curve.get();
  SetCurve(std::move(mock_curve));
  EXPECT_CALL(*curve, IsFinished()).WillOnce(testing::Return(true));
  EXPECT_CALL(*curve, GetScrollDelta(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, ScrollEndForSnapFling(true)).Times(1);
  controller_->Animate(base::TimeTicks() + base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(curve);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_CALL(*curve, IsFinished()).Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_CALL(mock_client_, ScrollEndForSnapFling(true)).Times(0);
  controller_->Animate(base::TimeTicks() + base::Seconds(2));
  testing::Mock::VerifyAndClearExpectations(curve);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, GSBNotFilteredAndResetsStateWhenActive) {
  SetActiveState();
  EXPECT_TRUE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kUpdate));

  EXPECT_CALL(mock_client_, ScrollEndForSnapFling(false)).Times(1);
  EXPECT_FALSE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kBegin));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_FALSE(controller_->FilterEventForSnap(
      SnapFlingController::GestureScrollType::kUpdate));
}

}  // namespace test
}  // namespace cc
