// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/gaussian_trainer.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chromeos/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

class GaussianTrainerTest : public testing::Test {
 public:
  GaussianTrainerTest()
      : global_curve_(MonotoneCubicSpline(log_lux_, global_brightness_)),
        personal_curve_(MonotoneCubicSpline(log_lux_, personal_brightness_)) {}

  void ResetModelWithParams(const std::map<std::string, std::string>& params) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kAutoScreenBrightness, params);

    gaussian_trainer_ = std::make_unique<GaussianTrainer>();
  }

  ~GaussianTrainerTest() override = default;

 protected:
  const std::vector<double> log_lux_ = {-4, -2, 0,  2,  4,  6,  8,
                                        10, 12, 14, 16, 18, 20, 22,
                                        24, 26, 28, 30, 32, 34, 36};
  const std::vector<double> global_brightness_ = {1,  5,  10, 15, 20, 25, 30,
                                                  35, 40, 45, 50, 55, 60, 65,
                                                  70, 75, 80, 85, 90, 95, 100};
  const std::vector<double> personal_brightness_ = {
      3,  8,  12, 17, 22, 27, 32, 37, 42, 46, 56,
      61, 66, 71, 76, 79, 81, 86, 91, 95, 100};

  // These values are set to not constrain anything (e.g. outliers). Individual
  // param will be overridden in unit tests.
  const std::map<std::string, std::string> default_params_{
      {"brightness_bound_scale", "100"},
      {"brightness_bound_offset", "100"},
      {"brightness_step_size", "100"},
      {"sigma", "0.1"},
      {"low_log_lux_threshold", "0"},
      {"min_grad_low_lux", "0"},
      {"min_grad", "0"},
      {"max_grad", "1"},
      {"min_brightness", "0"}};

  // Tests below generally test changes to the |ref_index_|'th entry in the
  // brightness curve.
  const size_t ref_index_ = 10;
  const double ref_log_lux_ = log_lux_[ref_index_];
  const double ref_global_brightness_ = global_brightness_[ref_index_];
  const double ref_personal_brightness_ = personal_brightness_[ref_index_];

  MonotoneCubicSpline global_curve_;
  MonotoneCubicSpline personal_curve_;
  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<GaussianTrainer> gaussian_trainer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GaussianTrainerTest);
};

// Tests the effect of |brightness_bound_scale| on outlier checks. A larger
// value would result in a data point less likely to be considered an outlier.
TEST_F(GaussianTrainerTest, OutlierBoundScale) {
  std::map<std::string, std::string> params = default_params_;
  const double bound_scale = 1.5;

  params["brightness_bound_scale"] = base::NumberToString(bound_scale);
  params["brightness_bound_offset"] = "0";

  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const double min_bound = ref_global_brightness_ / bound_scale;
  const double max_bound = ref_global_brightness_ * bound_scale;

  const TrainingDataPoint data_too_low = {min_bound - 5, min_bound - 10,
                                          ref_log_lux_, tick_clock_.NowTicks()};

  const TrainingDataPoint data_too_high = {
      max_bound + 5, max_bound + 10, ref_log_lux_, tick_clock_.NowTicks()};

  // |data_too_low| and |data_too_high| are both ignored. Hence there is no
  // change in the personal curve.
  const MonotoneCubicSpline trained_curve1 =
      gaussian_trainer_->Train({data_too_low, data_too_high});
  EXPECT_EQ(trained_curve1, personal_curve_);

  // Next increase |brightness_bound_scale|, so that the two training data
  // points are no longer outliers. A new curve will be trained.
  params["brightness_bound_scale"] = base::NumberToString(bound_scale * 100);
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve2 =
      gaussian_trainer_->Train({data_too_low, data_too_high});
  EXPECT_FALSE(trained_curve2 == personal_curve_);
  const std::vector<double> new_log_lux2 = trained_curve2.GetControlPointsX();

  for (size_t i = 0; i < personal_brightness_.size(); ++i) {
    EXPECT_DOUBLE_EQ(new_log_lux2[i], log_lux_[i]);
  }
}

// Tests the effect of |brightness_bound_offset| on outlier checks. A larger
// value would result in a data point less likely to be considered an outlier.
TEST_F(GaussianTrainerTest, OutlierBoundOffset) {
  std::map<std::string, std::string> params = default_params_;
  const double bound_offset = 40;

  params["brightness_bound_scale"] = "1";
  params["brightness_bound_offset"] = base::NumberToString(bound_offset);

  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const double min_bound = ref_global_brightness_ - bound_offset;
  const double max_bound = ref_global_brightness_ + bound_offset;

  const TrainingDataPoint data_too_low = {min_bound - 5, min_bound - 10,
                                          ref_log_lux_, tick_clock_.NowTicks()};

  const TrainingDataPoint data_too_high = {
      max_bound + 5, max_bound + 10, ref_log_lux_, tick_clock_.NowTicks()};

  // |data_too_low| and |data_too_high| are both ignored. Hence there is no
  // change in the personal curve.
  const MonotoneCubicSpline trained_curve1 =
      gaussian_trainer_->Train({data_too_low, data_too_high});
  EXPECT_EQ(trained_curve1, personal_curve_);

  // Next increase |brightness_bound_offset|, so that the two training data
  // points are no longer outliers. A new curve will be trained.
  params["brightness_bound_offset"] = base::NumberToString(bound_offset + 20);
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve2 =
      gaussian_trainer_->Train({data_too_low, data_too_high});
  EXPECT_FALSE(trained_curve2 == personal_curve_);
  const std::vector<double> new_log_lux2 = trained_curve2.GetControlPointsX();

  for (size_t i = 0; i < personal_brightness_.size(); ++i) {
    EXPECT_DOUBLE_EQ(new_log_lux2[i], log_lux_[i]);
  }
}

// Tests the effect of |brightness_step_size| on the training data point and
// hence the trained curve. A smaller value would lead to a narrower brightness
// change that is considered plausible. Hence changes on brightness curve will
// be smaller too.
TEST_F(GaussianTrainerTest, BrightnessStepSize) {
  // Brightness change occurs at a control point (|ref_log_lux_|).
  const TrainingDataPoint data = {ref_personal_brightness_ + 1,
                                  ref_personal_brightness_ + 20, ref_log_lux_,
                                  tick_clock_.NowTicks()};

  // First train the curve with |brightness_step_size| = 100.
  std::map<std::string, std::string> params = default_params_;
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve1 = gaussian_trainer_->Train({data});
  const std::vector<double> new_log_lux1 = trained_curve1.GetControlPointsX();
  const std::vector<double> new_brightness1 =
      trained_curve1.GetControlPointsY();

  // Next train the curve with a smaller |brightness_step_size|. Hence increase
  // in brightness adjustment is effectively capped.
  params["brightness_step_size"] = "0.2";
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve2 = gaussian_trainer_->Train({data});
  const std::vector<double> new_log_lux2 = trained_curve2.GetControlPointsX();
  const std::vector<double> new_brightness2 =
      trained_curve2.GetControlPointsY();

  EXPECT_EQ(new_log_lux1.size(), log_lux_.size());
  EXPECT_EQ(new_log_lux2.size(), log_lux_.size());

  for (size_t i = 0; i < personal_brightness_.size(); ++i) {
    EXPECT_DOUBLE_EQ(new_log_lux1[i], log_lux_[i]);
    EXPECT_DOUBLE_EQ(new_log_lux2[i], log_lux_[i]);

    if (i == ref_index_) {
      // At |ref_index_| brightness of |trained_curve1| should be strictly
      // bigger because it has a larger step size.
      EXPECT_GT(new_brightness1[i], new_brightness2[i]);
      EXPECT_GT(new_brightness2[i], personal_brightness_[i]);
    } else {
      // At other points, |trained_curve1| should be not smaller than
      // |trained_curve2|. The actual difference depends on |sigma|.
      EXPECT_GE(new_brightness1[i], new_brightness2[i]);
      EXPECT_GE(new_brightness2[i], personal_brightness_[i]);
    }
  }
}

// Tests the effect of |sigma| on the globalness/localness of a single data
// point on the entire curve. A larger value would result in more control points
// being updated (in addition to the one nearest to the training data).
TEST_F(GaussianTrainerTest, Sigma) {
  // Brightness change occurs at a control point (|ref_log_lux_|).
  const TrainingDataPoint data = {ref_personal_brightness_ + 1,
                                  ref_personal_brightness_ + 5, ref_log_lux_,
                                  tick_clock_.NowTicks()};

  // First train the curve with |sigma| = 0.1.
  std::map<std::string, std::string> params = default_params_;
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve1 = gaussian_trainer_->Train({data});
  const std::vector<double> new_log_lux1 = trained_curve1.GetControlPointsX();
  const std::vector<double> new_brightness1 =
      trained_curve1.GetControlPointsY();

  // Next train the curve with a larger |sigma|. Hence more control points have
  // larger brightness.
  params["sigma"] = "10";
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve2 = gaussian_trainer_->Train({data});
  const std::vector<double> new_log_lux2 = trained_curve2.GetControlPointsX();
  const std::vector<double> new_brightness2 =
      trained_curve2.GetControlPointsY();

  EXPECT_EQ(new_log_lux1.size(), log_lux_.size());
  EXPECT_EQ(new_log_lux2.size(), log_lux_.size());

  // Total brightness difference between |trained_curve2| and |trained_curve1|.
  double brightness_diff_21 = 0;
  for (size_t i = 0; i < personal_brightness_.size(); ++i) {
    EXPECT_DOUBLE_EQ(new_log_lux1[i], log_lux_[i]);
    EXPECT_DOUBLE_EQ(new_log_lux2[i], log_lux_[i]);

    if (i == ref_index_) {
      EXPECT_DOUBLE_EQ(new_brightness1[i], new_brightness2[i]);
      EXPECT_GT(new_brightness1[i], personal_brightness_[i]);
    } else {
      EXPECT_GE(new_brightness2[i], new_brightness1[i]);
      EXPECT_GE(new_brightness1[i], personal_brightness_[i]);
    }
    brightness_diff_21 += new_brightness2[i] - new_brightness1[i];
  }

  EXPECT_GT(brightness_diff_21, 0);
}

// Tests the effect of |max_grad| on the trained curve. A smaller value would
// lead to a flatter curve.
TEST_F(GaussianTrainerTest, MaxGrad) {
  // Brightness change occurs at a control point (|ref_log_lux_|).
  const TrainingDataPoint data = {ref_personal_brightness_ + 1,
                                  ref_personal_brightness_ + 20, ref_log_lux_,
                                  tick_clock_.NowTicks()};

  // First train the curve with |max_grad| = 1.
  std::map<std::string, std::string> params = default_params_;
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve1 = gaussian_trainer_->Train({data});
  const std::vector<double> new_log_lux1 = trained_curve1.GetControlPointsX();
  const std::vector<double> new_brightness1 =
      trained_curve1.GetControlPointsY();

  // Next train the curve with a smaller |max_grad|. Hence the curve will be
  // flatter.
  params["max_grad"] = "0.2";
  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve2 = gaussian_trainer_->Train({data});
  const std::vector<double> new_log_lux2 = trained_curve2.GetControlPointsX();
  const std::vector<double> new_brightness2 =
      trained_curve2.GetControlPointsY();

  EXPECT_EQ(new_log_lux1.size(), log_lux_.size());
  EXPECT_EQ(new_log_lux2.size(), log_lux_.size());

  // It's not guaranteed that each point on |trained_curve2| would have a
  // smaller slope than the corresponding point on |trained_curve1|. Hence we
  // check max slope of |trained_curve2| is larger than the min slope of
  // |trained_curve1|.
  double max_ratio1 = std::numeric_limits<double>::min();
  double max_ratio2 = std::numeric_limits<double>::min();

  for (size_t i = 0; i < personal_brightness_.size(); ++i) {
    EXPECT_DOUBLE_EQ(new_log_lux1[i], log_lux_[i]);
    EXPECT_DOUBLE_EQ(new_log_lux2[i], log_lux_[i]);

    if (i < personal_brightness_.size() - 1) {
      const double ratio1 = new_brightness1[i + 1] / new_brightness1[i];
      const double ratio2 = new_brightness2[i + 1] / new_brightness2[i];
      max_ratio1 = std::max(max_ratio1, ratio1);
      max_ratio2 = std::max(max_ratio2, ratio2);
    }
  }
  EXPECT_GT(max_ratio1, max_ratio2);
}

// The current curve isn't updated because training data point is consistent
// with existing model prediction.
TEST_F(GaussianTrainerTest, ConsistentModelPredictionNoCurveUpdate) {
  ResetModelWithParams(default_params_);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  // User increased brightness and target is lower than model prediction. Hence
  // no change to the curve.
  EXPECT_EQ(gaussian_trainer_->Train(
                {{ref_personal_brightness_ - 20, ref_personal_brightness_ - 10,
                  ref_log_lux_, tick_clock_.NowTicks()}}),
            personal_curve_);

  ResetModelWithParams(default_params_);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  // User decreased brightness and target is higher than model prediction. Hence
  // no change to the curve.
  EXPECT_EQ(gaussian_trainer_->Train(
                {{ref_personal_brightness_ + 20, ref_personal_brightness_ + 10,
                  ref_log_lux_, tick_clock_.NowTicks()}}),
            personal_curve_);
}

// Tests numerical results of a trained curve so that we could detect any
// unexpected changes when algorithm changes.
TEST_F(GaussianTrainerTest, TrainedCurveValue) {
  // Brightness change occurs at a control point (|ref_log_lux_|).
  const TrainingDataPoint data = {ref_personal_brightness_ + 1,
                                  ref_personal_brightness_ + 20, ref_log_lux_,
                                  tick_clock_.NowTicks()};

  const std::map<std::string, std::string> params{
      {"brightness_bound_scale", "1.5"},
      {"brightness_bound_offset", "40"},
      {"brightness_step_size", "0.2"},
      {"sigma", "1"},
      {"low_log_lux_threshold", "0"},
      {"min_grad_low_lux", "0"},
      {"min_grad", "0"},
      {"max_grad", "1"},
      {"min_brightness", "0"}};

  ResetModelWithParams(params);
  gaussian_trainer_->SetInitialCurves(global_curve_, personal_curve_);

  const MonotoneCubicSpline trained_curve = gaussian_trainer_->Train({data});
  const MonotoneCubicSpline expected_curve(
      log_lux_,
      {3.0,  8.0,  13.68, 20.52, 27.36, 34.2, 41.04, 47.88, 54.72, 61.56, 68.4,
       68.4, 68.4, 71.0,  76.0,  79.0,  81.0, 86.0,  91.0,  95.0,  100.0});
  EXPECT_EQ(trained_curve, expected_curve);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
