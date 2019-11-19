// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_finch_helper.h"

#include <map>
#include <set>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class HatsFinchHelperTest : public testing::Test {
 public:
  HatsFinchHelperTest() {}

  void SetFeatureParams(const base::FieldTrialParams& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kHappinessTrackingSystem, params);
  }

  base::FieldTrialParams CreateParamMap(std::string prob,
                                        std::string cycle_length,
                                        std::string start_date,
                                        std::string reset_survey,
                                        std::string reset) {
    base::FieldTrialParams params;
    params[HatsFinchHelper::kProbabilityParam] = prob;
    params[HatsFinchHelper::kSurveyCycleLengthParam] = cycle_length;
    params[HatsFinchHelper::kSurveyStartDateMsParam] = start_date;
    params[HatsFinchHelper::kResetSurveyCycleParam] = reset_survey;
    params[HatsFinchHelper::kResetAllParam] = reset;
    return params;
  }

 private:
  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;

 protected:
  TestingProfile profile_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HatsFinchHelperTest);
};

TEST_F(HatsFinchHelperTest, InitFinchSeed_ValidValues) {
  base::FieldTrialParams params =
      CreateParamMap("1.0", "7", "1475613895337", "false", "false");
  SetFeatureParams(params);

  HatsFinchHelper hats_finch_helper(&profile_);

  EXPECT_EQ(hats_finch_helper.probability_of_pick_, 1.0);
  EXPECT_EQ(hats_finch_helper.survey_cycle_length_, 7);
  EXPECT_EQ(hats_finch_helper.first_survey_start_date_,
            base::Time().FromJsTime(1475613895337LL));
  EXPECT_FALSE(hats_finch_helper.reset_survey_cycle_);
  EXPECT_FALSE(hats_finch_helper.reset_hats_);
}

TEST_F(HatsFinchHelperTest, InitFinchSeed_Invalidalues) {
  base::FieldTrialParams params =
      CreateParamMap("-0.1", "-1", "-1000", "false", "false");
  SetFeatureParams(params);

  base::Time current_time = base::Time::Now();
  HatsFinchHelper hats_finch_helper(&profile_);

  EXPECT_EQ(hats_finch_helper.probability_of_pick_, 0.0);
  EXPECT_EQ(hats_finch_helper.survey_cycle_length_, INT_MAX);
  EXPECT_GE(hats_finch_helper.first_survey_start_date_.ToJsTime(),
            2 * current_time.ToJsTime());
}

TEST_F(HatsFinchHelperTest, TestComputeNextDate) {
  base::FieldTrialParams params =
      CreateParamMap("0",
                     "7",  // 7 Days survey cycle length
                     "0", "false", "false");

  SetFeatureParams(params);

  base::Time current_time = base::Time::Now();

  HatsFinchHelper hats_finch_helper(&profile_);

  // Case 1
  base::Time start_date = current_time - base::TimeDelta::FromDays(10);
  hats_finch_helper.first_survey_start_date_ = start_date;
  base::Time expected_date =
      start_date +
      base::TimeDelta::FromDays(2 * hats_finch_helper.survey_cycle_length_);
  EXPECT_EQ(expected_date.ToJsTime(),
            hats_finch_helper.ComputeNextEndDate().ToJsTime());

  // Case 2
  base::Time future_time = current_time + base::TimeDelta::FromDays(10);
  hats_finch_helper.first_survey_start_date_ = future_time;
  expected_date = future_time + base::TimeDelta::FromDays(
                                    hats_finch_helper.survey_cycle_length_);
  EXPECT_EQ(expected_date.ToJsTime(),
            hats_finch_helper.ComputeNextEndDate().ToJsTime());
}

TEST_F(HatsFinchHelperTest, ResetSurveyCycle) {
  base::FieldTrialParams params =
      CreateParamMap("0.5", "7", "1475613895337", "true", "0");
  SetFeatureParams(params);

  int64_t initial_timestamp = base::Time::Now().ToInternalValue();
  PrefService* pref_service = profile_.GetPrefs();
  pref_service->SetBoolean(prefs::kHatsDeviceIsSelected, true);
  pref_service->SetInt64(prefs::kHatsSurveyCycleEndTimestamp,
                         initial_timestamp);

  base::Time current_time = base::Time::Now();
  HatsFinchHelper hats_finch_helper(&profile_);

  EXPECT_EQ(hats_finch_helper.probability_of_pick_, 0);
  EXPECT_EQ(hats_finch_helper.survey_cycle_length_, INT_MAX);
  EXPECT_GE(hats_finch_helper.first_survey_start_date_.ToJsTime(),
            2 * current_time.ToJsTime());

  EXPECT_FALSE(pref_service->GetBoolean(prefs::kHatsDeviceIsSelected));
  EXPECT_NE(pref_service->GetInt64(prefs::kHatsSurveyCycleEndTimestamp),
            initial_timestamp);
}

TEST_F(HatsFinchHelperTest, ResetHats) {
  base::FieldTrialParams params =
      CreateParamMap("0.5", "7", "1475613895337", "0", "true");
  SetFeatureParams(params);

  int64_t initial_timestamp = base::Time::Now().ToInternalValue();
  PrefService* pref_service = profile_.GetPrefs();
  pref_service->SetBoolean(prefs::kHatsDeviceIsSelected, true);
  pref_service->SetInt64(prefs::kHatsSurveyCycleEndTimestamp,
                         initial_timestamp);
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                         initial_timestamp);

  base::Time current_time = base::Time::Now();
  HatsFinchHelper hats_finch_helper(&profile_);

  EXPECT_EQ(hats_finch_helper.probability_of_pick_, 0);
  EXPECT_EQ(hats_finch_helper.survey_cycle_length_, INT_MAX);
  EXPECT_GE(hats_finch_helper.first_survey_start_date_.ToJsTime(),
            2 * current_time.ToJsTime());

  EXPECT_FALSE(pref_service->GetBoolean(prefs::kHatsDeviceIsSelected));
  EXPECT_NE(pref_service->GetInt64(prefs::kHatsSurveyCycleEndTimestamp),
            initial_timestamp);
  EXPECT_NE(pref_service->GetInt64(prefs::kHatsLastInteractionTimestamp),
            initial_timestamp);
}

}  // namespace chromeos
