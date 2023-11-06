// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_finch_helper.h"

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

// These values should match the param key values in the finch config file.
// static
const char HatsFinchHelper::kEnabledForGooglersParam[] = "enabled_for_googlers";
// static
const char HatsFinchHelper::kCustomClientDataParam[] = "custom_client_data";
// static
const char HatsFinchHelper::kProbabilityParam[] = "prob";
// static
const char HatsFinchHelper::kResetAllParam[] = "reset_all";
// static
const char HatsFinchHelper::kResetSurveyCycleParam[] = "reset_survey_cycle";
// static
const char HatsFinchHelper::kSurveyCycleLengthParam[] = "survey_cycle_length";
// static
const char HatsFinchHelper::kSurveyStartDateMsParam[] = "survey_start_date_ms";
// static
const char HatsFinchHelper::kTriggerIdParam[] = "trigger_id";

const char HatsFinchHelper::kHistogramNameParam[] = "histogram_name";

std::string HatsFinchHelper::GetTriggerID(const HatsConfig& hats_config) {
  DCHECK(base::FeatureList::IsEnabled(hats_config.feature));
  return base::GetFieldTrialParamValueByFeature(hats_config.feature,
                                                kTriggerIdParam);
}

// To enable UMA collection for a specific survey, the Finch configuration
// file for this survey should be updated to include a `histogram_name`
// parameter along side the `trigger_id` parameter. Without this
// `histogram_name` parameter specified, no survey-specific UMA data will be
// collected.
std::string HatsFinchHelper::GetHistogramName(const HatsConfig& hats_config) {
  DCHECK(base::FeatureList::IsEnabled(hats_config.feature));
  // Fetch the histogram name from the feature parameters, if it is assigned.
  // An empty string will be returned if the parameter is not set in Finch.
  // This value should be a valid histogram that has been registered in the
  // histograms.xml file, otherwise it will not be ingested by UMA.
  std::string histogram_name = base::GetFieldTrialParamValueByFeature(
      hats_config.feature, kHistogramNameParam);

  // Valid histogram names for HaTS/UMA integration must start with the
  // prefix "ChromeOS.HaTS.". This corresponds to the histogram definition
  // in the histograms.xml file.
  if (!base::StartsWith(histogram_name, "ChromeOS.HaTS.")) {
    LOG(ERROR) << "Invalid HaTS histogram name: " << histogram_name;
    return std::string();
  }

  return histogram_name;
}

std::string HatsFinchHelper::GetCustomClientDataAsString(
    const HatsConfig& hats_config) {
  DCHECK(base::FeatureList::IsEnabled(hats_config.feature));
  return base::GetFieldTrialParamValueByFeature(hats_config.feature,
                                                kCustomClientDataParam);
}

bool HatsFinchHelper::IsEnabledForGooglers(const HatsConfig& hats_config) {
  DCHECK(base::FeatureList::IsEnabled(hats_config.feature));
  return base::GetFieldTrialParamByFeatureAsBool(
      hats_config.feature, kEnabledForGooglersParam, false);
}

HatsFinchHelper::HatsFinchHelper(Profile* profile,
                                 const HatsConfig& hats_config)
    : profile_(profile), hats_config_(hats_config) {
  LoadFinchParamValues(hats_config);

  // Reset prefs related to survey cycle if the finch seed has the reset param
  // set. Do no futher op until a new finch seed with the reset flags unset is
  // received.
  // Warning: |reset_hats_| applies to all surveys.
  if (reset_survey_cycle_ || reset_hats_) {
    profile_->GetPrefs()->ClearPref(hats_config.cycle_end_timestamp_pref_name);
    profile_->GetPrefs()->ClearPref(hats_config.is_selected_pref_name);
    if (reset_hats_)
      profile_->GetPrefs()->ClearPref(prefs::kHatsLastInteractionTimestamp);
    return;
  }

  CheckForDeviceSelection();
}

HatsFinchHelper::~HatsFinchHelper() = default;

void HatsFinchHelper::LoadFinchParamValues(const HatsConfig& hats_config) {
  if (!base::FeatureList::IsEnabled(hats_config.feature))
    return;

  probability_of_pick_ = base::GetFieldTrialParamByFeatureAsDouble(
      hats_config.feature, kProbabilityParam, -1.0);

  if (probability_of_pick_ < 0.0 || probability_of_pick_ > 1.0) {
    LOG(ERROR) << "Invalid value for probability: " << probability_of_pick_;
    probability_of_pick_ = 0;
  }

  survey_cycle_length_ = base::GetFieldTrialParamByFeatureAsInt(
      hats_config.feature, kSurveyCycleLengthParam, 0);

  if (survey_cycle_length_ <= 0) {
    LOG(ERROR) << "Invalid value for survey cycle length: "
               << survey_cycle_length_;
    survey_cycle_length_ = INT_MAX;
  }

  double first_survey_start_date_ms = base::GetFieldTrialParamByFeatureAsDouble(
      hats_config.feature, kSurveyStartDateMsParam, -1.0);
  if (first_survey_start_date_ms < 0) {
    LOG(ERROR) << "Invalid timestamp for survey start date: "
               << first_survey_start_date_ms;
    // Set a random date in the distant future so that the survey never starts
    // until a new finch seed is received with the correct start date.
    first_survey_start_date_ms =
        2 * base::Time::Now().InMillisecondsFSinceUnixEpoch();
  }
  first_survey_start_date_ =
      base::Time().FromMillisecondsSinceUnixEpoch(first_survey_start_date_ms);

  trigger_id_ = GetTriggerID(hats_config);

  reset_survey_cycle_ = base::GetFieldTrialParamByFeatureAsBool(
      hats_config.feature, kResetSurveyCycleParam, false);

  reset_hats_ = base::GetFieldTrialParamByFeatureAsBool(hats_config.feature,
                                                        kResetAllParam, false);

  // Set every property to no op values if this is a reset finch seed.
  if (reset_survey_cycle_ || reset_hats_) {
    probability_of_pick_ = 0;
    survey_cycle_length_ = INT_MAX;
    first_survey_start_date_ = base::Time().FromMillisecondsSinceUnixEpoch(
        2 * base::Time::Now().InMillisecondsFSinceUnixEpoch());
  }
}

bool HatsFinchHelper::HasPreviousCycleEnded() {
  int64_t serialized_timestamp = profile_->GetPrefs()->GetInt64(
      hats_config_->cycle_end_timestamp_pref_name);
  base::Time recent_survey_cycle_end_time =
      base::Time::FromInternalValue(serialized_timestamp);
  return recent_survey_cycle_end_time < base::Time::Now();
}

base::Time HatsFinchHelper::ComputeNextEndDate() {
  base::Time start_date = first_survey_start_date_;
  base::TimeDelta delta = base::Days(survey_cycle_length_);
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
        profile_->GetPrefs()->GetBoolean(hats_config_->is_selected_pref_name);
    return;
  }

  // If the start date for the survey is in the future, do nothing.
  if (first_survey_start_date_ > base::Time::Now())
    return;

  // Start a new survey cycle and compute its end date.
  base::Time survey_cycle_end_date = ComputeNextEndDate();

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInt64(hats_config_->cycle_end_timestamp_pref_name,
                         survey_cycle_end_date.ToInternalValue());

  double rand_double = base::RandDouble();
  bool is_selected = false;
  if (rand_double < probability_of_pick_)
    is_selected = true;

  // Check if the trigger id is a valid string. Trigger IDs are a hash strings
  // of around 26 characters.
  is_selected = is_selected && (trigger_id_.length() > 15);

  pref_service->SetBoolean(hats_config_->is_selected_pref_name, is_selected);
  device_is_selected_for_cycle_ = is_selected;
}

}  // namespace ash
