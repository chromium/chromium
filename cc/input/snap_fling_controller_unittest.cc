// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_fling_controller.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
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
    feature_list_.InitAndDisableFeature(features::kSnapFlingDecayPrediction);
    controller_ = std::make_unique<SnapFlingController>(&mock_client_);
  }
  void SetCurve(std::unique_ptr<SnapFlingCurve> curve) {
    controller_->SetCurveForTest(std::move(curve));
  }
  void SetActiveState() { controller_->SetActiveStateForTest(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
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
  gsu.is_overscroll = false;

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
  gsu.is_overscroll = false;

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
  gsu.is_overscroll = false;

  EXPECT_CALL(mock_client_, GetSnapFlingInfoAndSetAnimatingSnapTarget(
                                testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(0);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(0);
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, DoesNotHandleOverscrollGSU) {
  SnapFlingController::GestureScrollUpdateInfo gsu;
  gsu.delta = gfx::Vector2dF(0, -10);
  gsu.is_in_inertial_phase = true;
  gsu.is_overscroll = true;

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

TEST_F(SnapFlingControllerTest, DecayPredictionFirstGSUDoesNotSnap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSnapFlingDecayPrediction);

  SnapFlingController::GestureScrollUpdateInfo gsu;
  gsu.delta = gfx::Vector2dF(0, -10);
  gsu.is_in_inertial_phase = true;
  gsu.is_overscroll = false;

  // GetSnapFlingInfoAndSetAnimatingSnapTarget should NOT be called.
  EXPECT_CALL(mock_client_, GetSnapFlingInfoAndSetAnimatingSnapTarget(
                                testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, DecayPredictionSecondGSUDecayingSnaps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSnapFlingDecayPrediction);

  SnapFlingController::GestureScrollUpdateInfo gsu1;
  gsu1.delta = gfx::Vector2dF(0, -10);
  gsu1.is_in_inertial_phase = true;
  gsu1.is_overscroll = false;

  // 1st GSU should not snap.
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu1));

  SnapFlingController::GestureScrollUpdateInfo gsu2;
  gsu2.delta = gfx::Vector2dF(0, -8);  // Decaying (8 < 10)
  gsu2.is_in_inertial_phase = true;
  gsu2.is_overscroll = false;

  // Decay factor = 8 / 10 = 0.8
  // Predicted displacement = 8 / (1 - 0.8) = 40
  // Vector: (0, -40)
  gfx::Vector2dF expected_displacement(0, -40);

  EXPECT_CALL(
      mock_client_,
      GetSnapFlingInfoAndSetAnimatingSnapTarget(
          gsu2.delta,
          testing::AllOf(
              testing::Property(&gfx::Vector2dF::x,
                                testing::FloatEq(expected_displacement.x())),
              testing::Property(&gfx::Vector2dF::y,
                                testing::FloatEq(expected_displacement.y()))),
          testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                               testing::SetArgPointee<3>(gfx::PointF(0, 100)),
                               testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(1);

  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(gsu2));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest,
       DecayPredictionSecondGSUNotDecayingDoesNotSnap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSnapFlingDecayPrediction);

  SnapFlingController::GestureScrollUpdateInfo gsu1;
  gsu1.delta = gfx::Vector2dF(0, -10);
  gsu1.is_in_inertial_phase = true;
  gsu1.is_overscroll = false;

  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu1));

  SnapFlingController::GestureScrollUpdateInfo gsu2;
  gsu2.delta = gfx::Vector2dF(0, -12);  // Not decaying (12 > 10)
  gsu2.is_in_inertial_phase = true;
  gsu2.is_overscroll = false;

  // Should not snap.
  EXPECT_CALL(mock_client_, GetSnapFlingInfoAndSetAnimatingSnapTarget(
                                testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu2));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  // Now send a 3rd GSU that is decaying relative to 2nd GSU (e.g. -9 < -12).
  SnapFlingController::GestureScrollUpdateInfo gsu3;
  gsu3.delta = gfx::Vector2dF(0, -9);  // Decaying relative to gsu2 (9 < 12)
  gsu3.is_in_inertial_phase = true;
  gsu3.is_overscroll = false;

  // Decay factor = 9 / 12 = 0.75
  // Predicted displacement = 9 / (1 - 0.75) = 36
  gfx::Vector2dF expected_displacement(0, -36);

  EXPECT_CALL(
      mock_client_,
      GetSnapFlingInfoAndSetAnimatingSnapTarget(
          gsu3.delta,
          testing::AllOf(
              testing::Property(&gfx::Vector2dF::x,
                                testing::FloatEq(expected_displacement.x())),
              testing::Property(&gfx::Vector2dF::y,
                                testing::FloatEq(expected_displacement.y()))),
          testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                               testing::SetArgPointee<3>(gfx::PointF(0, 100)),
                               testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(1);

  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(gsu3));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, DecayPredictionSlowDecaySnapsAfterTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSnapFlingDecayPrediction);

  SnapFlingController::GestureScrollUpdateInfo gsu1;
  gsu1.delta = gfx::Vector2dF(0, -10.f);
  gsu1.is_in_inertial_phase = true;
  gsu1.is_overscroll = false;

  // 1st GSU: no snap.
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu1));

  SnapFlingController::GestureScrollUpdateInfo gsu2;
  gsu2.delta = gfx::Vector2dF(0, -9.7f);  // f = 0.97 >= 0.96
  gsu2.is_in_inertial_phase = true;
  gsu2.is_overscroll = false;

  // 2nd GSU: slow decay, no snap yet.
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu2));

  SnapFlingController::GestureScrollUpdateInfo gsu3;
  gsu3.delta = gfx::Vector2dF(0, -9.409f);  // f = 0.97 >= 0.96
  gsu3.is_in_inertial_phase = true;
  gsu3.is_overscroll = false;

  // 3rd GSU: slow decay, no snap yet.
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu3));

  SnapFlingController::GestureScrollUpdateInfo gsu4;
  gsu4.delta = gfx::Vector2dF(0, -9.12673f);  // f = 0.97 >= 0.96
  gsu4.is_in_inertial_phase = true;
  gsu4.is_overscroll = false;

  // 4th GSU: slow decay, but we have 3 consecutive frames of decay. Should
  // snap!
  float decay = gsu4.delta.y() / gsu3.delta.y();
  gfx::Vector2dF expected_displacement(0, gsu4.delta.y() / (1.f - decay));

  EXPECT_CALL(
      mock_client_,
      GetSnapFlingInfoAndSetAnimatingSnapTarget(
          gsu4.delta,
          testing::AllOf(
              testing::Property(&gfx::Vector2dF::x,
                                testing::FloatEq(expected_displacement.x())),
              testing::Property(&gfx::Vector2dF::y,
                                testing::FloatEq(expected_displacement.y()))),
          testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                               testing::SetArgPointee<3>(gfx::PointF(0, 100)),
                               testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(1);

  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(gsu4));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(SnapFlingControllerTest, DecayPredictionSlowDecayResetOnAcceleration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSnapFlingDecayPrediction);

  SnapFlingController::GestureScrollUpdateInfo gsu1;
  gsu1.delta = gfx::Vector2dF(0, -10.f);
  gsu1.is_in_inertial_phase = true;
  gsu1.is_overscroll = false;
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu1));

  SnapFlingController::GestureScrollUpdateInfo gsu2;
  gsu2.delta = gfx::Vector2dF(0, -9.7f);  // decay_frames = 1
  gsu2.is_in_inertial_phase = true;
  gsu2.is_overscroll = false;
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu2));

  SnapFlingController::GestureScrollUpdateInfo gsu3;
  gsu3.delta =
      gfx::Vector2dF(0, -9.8f);  // accelerating! f >= 1. decay_frames -> 0
  gsu3.is_in_inertial_phase = true;
  gsu3.is_overscroll = false;
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu3));

  SnapFlingController::GestureScrollUpdateInfo gsu4;
  gsu4.delta = gfx::Vector2dF(0, -9.506f);  // decay_frames = 1 (f = 0.97)
  gsu4.is_in_inertial_phase = true;
  gsu4.is_overscroll = false;
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu4));

  SnapFlingController::GestureScrollUpdateInfo gsu5;
  gsu5.delta = gfx::Vector2dF(0, -9.22082f);  // decay_frames = 2 (f = 0.97)
  gsu5.is_in_inertial_phase = true;
  gsu5.is_overscroll = false;
  EXPECT_FALSE(controller_->HandleGestureScrollUpdate(gsu5));

  // 6th GSU (4th after reset) should snap.
  SnapFlingController::GestureScrollUpdateInfo gsu6;
  gsu6.delta = gfx::Vector2dF(0, -8.9442f);  // decay_frames = 3 (f = 0.97)
  gsu6.is_in_inertial_phase = true;
  gsu6.is_overscroll = false;

  float decay = gsu6.delta.y() / gsu5.delta.y();
  gfx::Vector2dF expected_displacement(0, gsu6.delta.y() / (1.f - decay));

  EXPECT_CALL(
      mock_client_,
      GetSnapFlingInfoAndSetAnimatingSnapTarget(
          gsu6.delta,
          testing::AllOf(
              testing::Property(&gfx::Vector2dF::x,
                                testing::FloatEq(expected_displacement.x())),
              testing::Property(&gfx::Vector2dF::y,
                                testing::FloatEq(expected_displacement.y()))),
          testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                               testing::SetArgPointee<3>(gfx::PointF(0, 100)),
                               testing::Return(true)));
  EXPECT_CALL(mock_client_, RequestAnimationForSnapFling()).Times(1);
  EXPECT_CALL(mock_client_, ScrollByForSnapFling(testing::_)).Times(1);

  EXPECT_TRUE(controller_->HandleGestureScrollUpdate(gsu6));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

}  // namespace test
}  // namespace cc
