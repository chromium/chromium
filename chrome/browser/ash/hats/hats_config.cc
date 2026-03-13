// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_config.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/time/time.h"

namespace ash {

namespace {
constexpr int kMinDaysThreshold = 0;  // HaTS Onboarding Experience is immediate
}  // namespace

HatsConfig::HatsConfig(const base::Feature& feature,
                       const base::TimeDelta& new_device_threshold,
                       const char* const is_selected_pref_name,
                       const char* const cycle_end_timestamp_pref_name)
    : feature(feature),
      new_device_threshold(new_device_threshold),
      is_selected_pref_name(is_selected_pref_name),
      cycle_end_timestamp_pref_name(cycle_end_timestamp_pref_name),
      survey_last_interaction_timestamp_pref_name(nullptr),
      threshold_time(base::TimeDelta()),
      prioritized(false) {
  DCHECK(new_device_threshold.InDaysFloored() >= kMinDaysThreshold);
}

HatsConfig::HatsConfig(
    const base::Feature& feature,
    const base::TimeDelta& new_device_threshold,
    const char* const is_selected_pref_name,
    const char* const cycle_end_timestamp_pref_name,
    const char* const survey_last_interaction_timestamp_pref_name,
    const base::TimeDelta& threshold_time)
    : feature(feature),
      new_device_threshold(new_device_threshold),
      is_selected_pref_name(is_selected_pref_name),
      cycle_end_timestamp_pref_name(cycle_end_timestamp_pref_name),
      survey_last_interaction_timestamp_pref_name(
          survey_last_interaction_timestamp_pref_name),
      threshold_time(threshold_time),
      prioritized(true) {
  DCHECK(new_device_threshold.InDaysFloored() >= kMinDaysThreshold);
}

// General Survey -- shown after login
const HatsConfig kHatsGeneralSurvey = {
    ash::features::kHappinessTrackingSystem,   // feature
    base::Days(7),                             // new_device_threshold
    ash::prefs::kHatsDeviceIsSelected,         // is_selected_pref_name
    ash::prefs::kHatsSurveyCycleEndTimestamp,  // cycle_end_timestamp_pref_name
};

// ENT Survey -- shown after login, along with the General Survey
const HatsConfig kHatsEntSurvey = {
    ash::features::kHappinessTrackingSystemEnt,  // feature
    base::Days(7),                               // new_device_threshold
    ash::prefs::kHatsEntDeviceIsSelected,        // is_selected_pref_name
    ash::prefs::kHatsEntSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Stability Survey -- shown after login, along with the General Survey
const HatsConfig kHatsStabilitySurvey = {
    ash::features::kHappinessTrackingSystemStability,  // feature
    base::Days(7),                                     // new_device_threshold
    ash::prefs::kHatsStabilityDeviceIsSelected,        // is_selected_pref_name
    ash::prefs::
        kHatsStabilitySurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Performance Survey -- shown after login, along with the General Survey
const HatsConfig kHatsPerformanceSurvey = {
    ash::features::kHappinessTrackingSystemPerformance,  // feature
    base::Days(7),                                       // new_device_threshold
    ash::prefs::kHatsPerformanceDeviceIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsPerformanceSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Onboarding Experience Survey -- shown after completing the Onboarding Dialog
const HatsConfig kHatsOnboardingSurvey = {
    ash::features::kHappinessTrackingSystemOnboarding,  // feature
    base::Minutes(30),                                  // new_device_threshold
    ash::prefs::kHatsOnboardingDeviceIsSelected,        // is_selected_pref_name
    ash::prefs::
        kHatsOnboardingSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// ARC++ Games Survey -- shown after a user played a top XX ARC++ game
const HatsConfig kHatsArcGamesSurvey = {
    ash::features::kHappinessTrackingSystemArcGames,  // feature
    base::Days(7),                                    // new_device_threshold
    ash::prefs::kHatsArcGamesDeviceIsSelected,        // is_selected_pref_name
    ash::prefs::kHatsArcGamesSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Audio Survey -- shown after a user closed an audio stream living for more
// than 3 minutes
const HatsConfig kHatsAudioSurvey = {
    ash::features::kHappinessTrackingSystemAudio,  // feature
    base::Days(90),                                // new_device_threshold
    ash::prefs::kHatsAudioDeviceIsSelected,        // is_selected_pref_name
    ash::prefs::kHatsAudioSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

const HatsConfig kHatsAudioOutputProcSurvey = {
    ash::features::kHappinessTrackingSystemAudioOutputProc,  // feature
    base::Days(7),                                     // new_device_threshold
    ash::prefs::kHatsAudioOutputProcDeviceIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsAudioOutputProcSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Bluetooth Audio Survey -- shown after the user closed an audio stream
// sent to a Bluetooth device after listening for more than one minute.
const HatsConfig kHatsBluetoothAudioSurvey = {
    ash::features::kHappinessTrackingSystemBluetoothAudio,  // feature
    base::Days(90),                                   // new_device_threshold
    ash::prefs::kHatsBluetoothAudioDeviceIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsBluetoothAudioSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};
// Personalization Avatar Survey -- shown 60 seconds after a user closes the
// Avatar selection page of either OS Settings or Personalization Hub, depending
// on whether PersonalizationHub feature is enabled.
const HatsConfig kHatsPersonalizationAvatarSurvey = {
    ash::features::kHappinessTrackingPersonalizationAvatar,  // feature
    base::Days(1),  // new_device_threshold
    ash::prefs::
        kHatsPersonalizationAvatarSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsPersonalizationAvatarSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Personalization Screensaver Survey -- shown 60 seconds after a user closes
// the Screensaver settings page of either OS Settings or Personalization Hub,
// depending on whether PersonalizationHub feature is enabled.
const HatsConfig kHatsPersonalizationScreensaverSurvey = {
    ash::features::kHappinessTrackingPersonalizationScreensaver,  // feature
    base::Days(1),  // new_device_threshold
    ash::prefs::
        kHatsPersonalizationScreensaverSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsPersonalizationScreensaverSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Personalization Wallpaper Survey -- shown 60 seconds after a user closes the
// Wallpaper subpage of the Personalization App.
const HatsConfig kHatsPersonalizationWallpaperSurvey = {
    ash::features::kHappinessTrackingPersonalizationWallpaper,  // feature
    base::Days(1),  // new_device_threshold
    ash::prefs::
        kHatsPersonalizationWallpaperSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsPersonalizationWallpaperSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// MediaApp PDF Editing experience survey -- shown after a user clicks `Save`
// after editing a PDF in the MediaApp (Gallery), and the save is complete.
const HatsConfig kHatsMediaAppPdfSurvey = {
    ash::features::kHappinessTrackingMediaAppPdf,  // feature
    base::Days(7),                                 // new_device_threshold
    ash::prefs::kHatsMediaAppPdfIsSelected,        // hatsIsSelectedPrefName
    ash::prefs::kHatsMediaAppPdfCycleEndTs,  // hatsCycleEndTimestampPrefName
};

// Camera App Survey -- shown after an user captured a photo/video or left the
// app with session > 15 seconds.
const HatsConfig kHatsCameraAppSurvey = {
    ash::features::kHappinessTrackingSystemCameraApp,  // feature
    base::Days(90),                                    // new_device_threshold
    ash::prefs::kHatsCameraAppDeviceIsSelected,        // is_selected_pref_name
    ash::prefs::
        kHatsCameraAppSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Chromebook Video/Image Editing/Viewing experience survey -- shown after a
// user opens and then subsequently closes the Google Photos Android App.
const HatsConfig kHatsPhotosExperienceSurvey = {
    ash::features::kHappinessTrackingPhotosExperience,  // feature
    base::Days(7),                                      // new_device_threshold
    ash::prefs::kHatsPhotosExperienceIsSelected,  // hatsIsSelectedPrefName
    ash::prefs::
        kHatsPhotosExperienceCycleEndTs,  // hatsCycleEndTimestampPrefName
};

// General Camera Survey -- shown after camera is closed after being open for
// at least 3 minutes by using any app (e.g. Chrome or Android app).
const HatsConfig kHatsGeneralCameraSurvey = {
    ash::features::kHappinessTrackingGeneralCamera,  // feature
    base::Days(90),                                  // new_device_threshold
    ash::prefs::kHatsGeneralCameraIsSelected,        // is_selected_pref_name
    ash::prefs::
        kHatsGeneralCameraSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Prioritized General Camera Survey -- shown after camera is closed after being
// open for at least 15 seconds by using any app (e.g. Chrome or Android app).
const HatsConfig kHatsGeneralCameraPrioritizedSurvey = {
    // feature
    ash::features::kHappinessTrackingGeneralCameraPrioritized,
    // new_device_threshold
    base::Days(7),
    // is_selected_pref_name
    ash::prefs::kHatsGeneralCameraPrioritizedIsSelected,
    // cycle_end_timestamp_pref_name
    ash::prefs::kHatsGeneralCameraPrioritizedSurveyCycleEndTs,
    // survey_last_interaction_timestamp_pref_name
    ash::prefs::kHatsGeneralCameraPrioritizedLastInteractionTimestamp,
    // threshold_time
    base::Days(120),
};

// Bluetooth revamp experience survey -- shown 5 mins after interacting with new
// Bluetooth UI surfaces.
const HatsConfig kHatsBluetoothRevampSurvey = {
    ash::features::kHappinessTrackingSystemBluetoothRevamp,  // feature
    base::Days(1),                               // new_device_threshold
    ash::prefs::kHatsBluetoothRevampIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsBluetoothRevampCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Battery life experience survey -- shown after login.
const HatsConfig kHatsBatteryLifeSurvey = {
    ash::features::kHappinessTrackingSystemBatteryLife,  // feature
    base::Days(7),                                       // new_device_threshold
    ash::prefs::kHatsBatteryLifeIsSelected,  // is_selected_pref_name
    ash::prefs::kHatsBatteryLifeCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Peripherals experience survey -- shown after login.
const HatsConfig kHatsPeripheralsSurvey = {
    ash::features::kHappinessTrackingSystemPeripherals,  // feature
    base::Days(7),                                       // new_device_threshold
    ash::prefs::kHatsPeripheralsIsSelected,  // is_selected_pref_name
    ash::prefs::kHatsPeripheralsCycleEndTs,  // cycle_end_timestamp_pref_name
};

// OS Settings Survey -- shown [5-30] seconds after a user removes focus from
// Settings or closes the Settings app, if user has used Search, it will add it
// as a Product Specific Data (PSD).
const HatsConfig kHatsOsSettingsSearchSurvey = {
    ash::features::kHappinessTrackingOsSettingsSearch,  // feature
    base::Days(1),                                      // new_device_threshold
    ash::prefs::kHatsOsSettingsSearchSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsOsSettingsSearchSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Borealis games survey -- Shown after a Steam game exits.
const HatsConfig kHatsBorealisGamesSurvey = {
    ash::features::kHappinessTrackingBorealisGames,  // feature
    base::Days(1),                                   // new_device_threshold
    ash::prefs::kHatsBorealisGamesSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsBorealisGamesSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
    ash::prefs::kHatsBorealisGamesLastInteractionTimestamp,
    // survey_last_interaction_timestamp_pref_name
    base::Days(7),  // threshold_time
};

// Launcher survey -- Shown after a user opens the launcher for the first time.
// This survey is enabled for 25% of users.
const HatsConfig kHatsLauncherAppsFindingSurvey = {
    ash::features::kHappinessTrackingLauncherAppsFinding,  // feature
    base::Hours(2),                                 // new_device_threshold
    ash::prefs::kHatsLauncherAppsSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsLauncherAppsSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Launcher survey -- Shown after a user opens the launcher for the first time.
// This survey is enabled for 75% of users.
const HatsConfig kHatsLauncherAppsNeedingSurvey = {
    ash::features::kHappinessTrackingLauncherAppsNeeding,  // feature
    base::Hours(2),                                 // new_device_threshold
    ash::prefs::kHatsLauncherAppsSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::
        kHatsLauncherAppsSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Office integration survey -- Shown after the user opens an Office file:
// For MS365 and Docs/Sheets/Slides, shown when the app is inactive or closed.
// For QuickOffice, shown 1 minute after launch.
const HatsConfig kHatsOfficeSurvey = {
    ash::features::kHappinessTrackingOffice,  // feature
    base::Days(1),                            // new_device_threshold
    ash::prefs::kHatsOfficeSurveyIsSelected,  // is_selected_pref_name
    ash::prefs::kHatsOfficeSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

}  // namespace ash
