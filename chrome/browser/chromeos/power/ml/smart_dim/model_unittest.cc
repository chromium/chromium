// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/smart_dim/model_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chromeos/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace chromeos {
namespace power {
namespace ml {

class SmartDimModelImplTest : public testing::Test {
 public:
  SmartDimModelImplTest() {
    // Bucketize to 95.
    features_.set_battery_percent(96.0);
    features_.set_device_management(UserActivityEvent::Features::UNMANAGED);
    features_.set_device_mode(UserActivityEvent::Features::CLAMSHELL);
    features_.set_device_type(UserActivityEvent::Features::CHROMEBOOK);
    // Bucketize to 200.
    features_.set_key_events_in_last_hour(290);
    features_.set_last_activity_day(UserActivityEvent::Features::THU);
    // Bucketize to 7.
    features_.set_last_activity_time_sec(25920);
    // Bucketize to 7.
    features_.set_last_user_activity_time_sec(25920);
    // Bucketize to 2000.
    features_.set_mouse_events_in_last_hour(2600);
    features_.set_on_battery(false);
    features_.set_previous_negative_actions_count(3);
    features_.set_previous_positive_actions_count(0);
    features_.set_recent_time_active_sec(190);
    features_.set_video_playing_time_sec(0);
    features_.set_on_to_dim_sec(30);
    features_.set_dim_to_screen_off_sec(10);
    features_.set_time_since_last_key_sec(30);
    features_.set_time_since_last_mouse_sec(688);
    // Bucketize to 900.
    features_.set_time_since_video_ended_sec(1100);
    features_.set_has_form_entry(false);
    features_.set_source_id(123);  // not used.
    features_.set_engagement_score(40);
    features_.set_tab_domain("//mail.google.com");
  }

  ~SmartDimModelImplTest() override = default;

 protected:
  float CalculateInactivityScore(const UserActivityEvent::Features& features) {
    float inactivity_score;
    SmartDimModelResult result =
        smart_dim_model_.CalculateInactivityScore(features, &inactivity_score);
    EXPECT_EQ(SmartDimModelResult::kSuccess, result);
    return inactivity_score;
  }

  UserActivityEvent::Features features_;
  SmartDimModelImpl smart_dim_model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmartDimModelImplTest);
};

TEST_F(SmartDimModelImplTest, Basic) {
  const float inactivity_score = CalculateInactivityScore(features_);
  // Score has been calculated outside of chrome.
  EXPECT_FLOAT_EQ(-3.6615708, inactivity_score);
}

TEST_F(SmartDimModelImplTest, OptionalFeaturesMissing) {
  features_.clear_battery_percent();
  features_.clear_time_since_last_key_sec();
  features_.clear_time_since_last_mouse_sec();
  features_.clear_time_since_video_ended_sec();
  features_.clear_source_id();
  features_.clear_has_form_entry();
  features_.clear_engagement_score();
  features_.clear_tab_domain();

  const float inactivity_score = CalculateInactivityScore(features_);
  // Score has been calculated outside of chrome.
  EXPECT_FLOAT_EQ(-1.9680425, inactivity_score);
}

TEST_F(SmartDimModelImplTest, ShouldNotDim) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", base::NumberToString(-0.1)}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);

  const UserActivityEvent::ModelPrediction prediction =
      smart_dim_model_.ShouldDim(features_);
  EXPECT_EQ(UserActivityEvent::ModelPrediction::NO_DIM, prediction.response());
  EXPECT_EQ(47, prediction.decision_threshold());
  EXPECT_EQ(2, prediction.inactivity_score());
}

TEST_F(SmartDimModelImplTest, ShouldDim) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", base::NumberToString(-10)}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);

  const UserActivityEvent::ModelPrediction prediction =
      smart_dim_model_.ShouldDim(features_);
  EXPECT_EQ(UserActivityEvent::ModelPrediction::DIM, prediction.response());
  EXPECT_EQ(0, prediction.decision_threshold());
  EXPECT_EQ(2, prediction.inactivity_score());
}

TEST_F(SmartDimModelImplTest, ModelError) {
  // Model parameter is undefined, which would trigger a model error.
  const UserActivityEvent::ModelPrediction prediction =
      smart_dim_model_.ShouldDim(features_);
  EXPECT_EQ(UserActivityEvent::ModelPrediction::MODEL_ERROR,
            prediction.response());
  EXPECT_FALSE(prediction.has_decision_threshold());
  EXPECT_FALSE(prediction.has_inactivity_score());
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
