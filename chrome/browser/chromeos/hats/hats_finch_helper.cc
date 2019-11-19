// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_finch_helper.h"

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

// These values should match the param key values in the finch config file.
// static
const char HatsFinchHelper::kProbabilityParam[] = "prob";
// static
const char HatsFinchHelper::kSurveyCycleLengthParam[] = "survey_cycle_length";
// static
const char HatsFinchHelper::kSurveyStartDateMsParam[] = "survey_start_date_ms";
// static
const char HatsFinchHelper::kResetSurveyCycleParam[] = "reset_survey_cycle";
// static
const char HatsFinchHelper::kResetAllParam[] = "reset_all";

HatsFinchHelper::HatsFinchHelper(Profile* profile) : profile_(profile) {
  LoadFinchParamValues();

  // Reset prefs related to survey cycle if the finch seed has the reset param
  // set. Do no futher op until a new finch seed with the reset flags unset is
  // received.
  if (reset_survey_cycle_ || reset_hats_) {
    profile_->GetPrefs()->ClearPref(prefs::kHatsSurveyCycleEndTimestamp);
    profile_->GetPrefs()->ClearPref(prefs::kHatsDeviceIsSelected);
    if (reset_hats_)
      profile_->GetPrefs()->ClearPref(prefs::kHatsLastInteractionTimestamp);
    return;
  }

  CheckForDeviceSelection();
}

HatsFinchHelper::~HatsFinchHelper() {}

void HatsFinchHelper::LoadFinchParamValues() {
  const auto& feature = features::kHappinessTrackingSystem;
  if (!base::FeatureList::IsEnabled(feature))
    return;

  probability_of_pick_ = base::GetFieldTrialParamByFeatureAsDouble(
      feature, kProbabilityParam, -1.0);

  if (probability_of_pick_ < 0.0 || probability_of_pick_ > 1.0) {
    LOG(ERROR) << "Invalid value for probability: " << probability_of_pick_;
    probability_of_pick_ = 0;
  }

  survey_cycle_length_ = base::GetFieldTrialParamByFeatureAsInt(
      feature, kSurveyCycleLengthParam, 0);

  if (survey_cycle_length_ <= 0) {
    LOG(ERROR) << "Invalid value for survey cycle length: "
               << survey_cycle_length_;
    survey_cycle_length_ = INT_MAX;
  }

  double first_survey_start_date_ms = base::GetFieldTrialParamByFeatureAsDouble(
      feature, kSurveyStartDateMsParam, -1.0);
  if (first_survey_start_date_ms < 0) {
    LOG(ERROR) << "Invalid timestamp for survey start date: "
               << first_survey_start_date_ms;
    // Set a random date in the distant future so that the survey never starts
    // until a new finch seed is received with the correct start date.
    first_survey_start_date_ms = 2 * base::Time::Now().ToJsTime();
  }
  first_survey_start_date_ =
      base::Time().FromJsTime(first_survey_start_date_ms);

  reset_survey_cycle_ = base::GetFieldTrialParamByFeatureAsBool(
      feature, kResetSurveyCycleParam, false);

  reset_hats_ =
      base::GetFieldTrialParamByFeatureAsBool(feature, kResetAllParam, false);

  // Set every property to no op values if this is a reset finch seed.
  if (reset_survey_cycle_ || reset_hats_) {
    probability_of_pick_ = 0;
    survey_cycle_length_ = INT_MAX;
    first_survey_start_date_ =
        base::Time().FromJsTime(2 * base::Time::Now().ToJsTime());
  }
}

bool HatsFinchHelper::HasPreviousCycleEnded() {
  int64_t serialized_timestamp =
      profile_->GetPrefs()->GetInt64(prefs::kHatsSurveyCycleEndTimestamp);
  base::Time recent_survey_cycle_end_time =
      base::Time::FromInternalValue(serialized_timestamp);
  return recent_survey_cycle_end_time < base::Time::Now();
}

base::Time HatsFinchHelper::ComputeNextEndDate() {
  base::Time start_date = first_survey_start_date_;
  base::TimeDelta delta = base::TimeDelta::FromDays(survey_cycle_length_);
  do {
    start_date += delta;
  } while (start_date < base::Time::Now());
  return start_date;
}

void HatsFinchHelper::CheckForDeviceSelection() {
  device_is_selected_for_cycle_ = false;

  // The dice is rolled only once per survey cycle. If it has already been done
  // for the current cycle, then return the stored value of the result.
  if (!HasPreviousCycleEnded()) {
    device_is_selected_for_cycle_ =
        profile_->GetPrefs()->GetBoolean(prefs::kHatsDeviceIsSelected);
    return;
  }

  // If the start date for the survey is in the future, do nothing.
  if (first_survey_start_date_ > base::Time::Now())
    return;

  // Start a new survey cycle and compute its end date.
  base::Time survey_cycle_end_date = ComputeNextEndDate();

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInt64(prefs::kHatsSurveyCycleEndTimestamp,
                         survey_cycle_end_date.ToInternalValue());

  double rand_double = base::RandDouble();
  bool is_selected = false;
  if (rand_double < probability_of_pick_)
    is_selected = true;
  pref_service->SetBoolean(prefs::kHatsDeviceIsSelected, is_selected);
  device_is_selected_for_cycle_ = is_selected;
}

}  // namespace chromeos
