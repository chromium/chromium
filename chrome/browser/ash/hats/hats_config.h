// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_HATS_HATS_CONFIG_H_
#define CHROME_BROWSER_ASH_HATS_HATS_CONFIG_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"

namespace ash {

struct HatsConfig {
  HatsConfig(const base::Feature& feature,
             const base::TimeDelta& new_device_threshold,
             const char* const is_selected_pref_name,
             const char* const cycle_end_timestamp_pref_name);
  // Using this constructor will set the survey to be prioritized.
  // Note: The number of prioritized survey should be very limited,
  // otherwise none of them will be so prioritized.
  HatsConfig(const base::Feature& feature,
             const base::TimeDelta& new_device_threshold,
             const char* const is_selected_pref_name,
             const char* const cycle_end_timestamp_pref_name,
             const char* const survey_last_interaction_timestamp_pref_name,
             const base::TimeDelta& threshold_time);
  HatsConfig(const HatsConfig&) = delete;
  HatsConfig& operator=(const HatsConfig&) = delete;

  // Chrome OS-level feature (switch) we use to retrieve whether this HaTS
  // survey is enabled or not, and its parameters.
  // This field is not a raw_ref<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const base::Feature& feature;

  // Minimum amount of time after initial login or oobe after which we can show
  // the HaTS notification.
  const base::TimeDelta new_device_threshold;

  // Preference name for a boolean that stores whether we were selected for the
  // current survey cycle.
  const char* const is_selected_pref_name;

  // Preference name for an int64 that stores the current survey cycle end.
  const char* const cycle_end_timestamp_pref_name;

  // Preference name for an int64 that stores the last time that the user
  // has interacted with this particular survey.
  const char* const survey_last_interaction_timestamp_pref_name;

  // Minimum amount of time after a user interacts with this survey after which
  // we can show this survey again. Only for prioritized HaTS, as their global
  // cooldown can be very short.
  const base::TimeDelta threshold_time;

  // Prioritized HaTS has its own separate global cooldown period.
  // To make the survey actually "prioritized", ideally:
  // 1. The global cooldown period must be much shorter than the general HaTS,
  //    configurable as |kPrioritizedHatsThreshold| in
  //    hats_notification_controller.cc.
  // 2. The probability of being chosen should be higher than the general HaTS,
  //    e.g. higher than 1%.
  // 3. There should be very limited number of prioritized HaTS.
  const bool prioritized;
};

// CrOS HaTS configs are declared here and defined in hats_config.cc
extern const HatsConfig kHatsGeneralSurvey;
extern const HatsConfig kHatsEntSurvey;
extern const HatsConfig kHatsStabilitySurvey;
extern const HatsConfig kHatsPerformanceSurvey;
extern const HatsConfig kHatsOnboardingSurvey;
extern const HatsConfig kHatsArcGamesSurvey;
extern const HatsConfig kHatsAudioSurvey;
extern const HatsConfig kHatsAudioOutputProcSurvey;
extern const HatsConfig kHatsBluetoothAudioSurvey;
extern const HatsConfig kHatsPersonalizationAvatarSurvey;
extern const HatsConfig kHatsPersonalizationScreensaverSurvey;
extern const HatsConfig kHatsPersonalizationWallpaperSurvey;
extern const HatsConfig kHatsMediaAppPdfSurvey;
extern const HatsConfig kHatsCameraAppSurvey;
extern const HatsConfig kHatsPhotosExperienceSurvey;
extern const HatsConfig kHatsGeneralCameraSurvey;
extern const HatsConfig kHatsGeneralCameraPrioritizedSurvey;
extern const HatsConfig kHatsBluetoothRevampSurvey;
extern const HatsConfig kHatsBatteryLifeSurvey;
extern const HatsConfig kHatsPeripheralsSurvey;
extern const HatsConfig kPrivacyHubPostLaunchSurvey;
extern const HatsConfig kHatsOsSettingsSearchSurvey;
extern const HatsConfig kHatsBorealisGamesSurvey;
extern const HatsConfig kHatsLauncherAppsFindingSurvey;
extern const HatsConfig kHatsLauncherAppsNeedingSurvey;
extern const HatsConfig kHatsOfficeSurvey;

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_HATS_HATS_CONFIG_H_
