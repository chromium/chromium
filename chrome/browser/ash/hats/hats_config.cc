// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_config.h"

#include "base/time/time.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"

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
    ::features::kHappinessTrackingSystem,  // feature
    base::Days(7),                         // new_device_threshold
    prefs::kHatsDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsSurveyCycleEndTimestamp,   // cycle_end_timestamp_pref_name
};

// ENT Survey -- shown after login, along with the General Survey
const HatsConfig kHatsEntSurvey = {
    ::features::kHappinessTrackingSystemEnt,  // feature
    base::Days(7),                            // new_device_threshold
    prefs::kHatsEntDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsEntSurveyCycleEndTs,          // cycle_end_timestamp_pref_name
};

// Stability Survey -- shown after login, along with the General Survey
const HatsConfig kHatsStabilitySurvey = {
    ::features::kHappinessTrackingSystemStability,  // feature
    base::Days(7),                                  // new_device_threshold
    prefs::kHatsStabilityDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsStabilitySurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Performance Survey -- shown after login, along with the General Survey
const HatsConfig kHatsPerformanceSurvey = {
    ::features::kHappinessTrackingSystemPerformance,  // feature
    base::Days(7),                                    // new_device_threshold
    prefs::kHatsPerformanceDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsPerformanceSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Onboarding Experience Survey -- shown after completing the Onboarding Dialog
const HatsConfig kHatsOnboardingSurvey = {
    ::features::kHappinessTrackingSystemOnboarding,  // feature
    base::Minutes(30),                               // new_device_threshold
    prefs::kHatsOnboardingDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsOnboardingSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// ARC++ Games Survey -- shown after a user played a top XX ARC++ game
const HatsConfig kHatsArcGamesSurvey = {
    ::features::kHappinessTrackingSystemArcGames,  // feature
    base::Days(7),                                 // new_device_threshold
    prefs::kHatsArcGamesDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsArcGamesSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Audio Survey -- shown after a user closed an audio stream living for more
// than 3 minutes
const HatsConfig kHatsAudioSurvey = {
    ::features::kHappinessTrackingSystemAudio,  // feature
    base::Days(90),                             // new_device_threshold
    prefs::kHatsAudioDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsAudioSurveyCycleEndTs,          // cycle_end_timestamp_pref_name
};

const HatsConfig kHatsAudioOutputProcSurvey = {
    ::features::kHappinessTrackingSystemAudioOutputProc,  // feature
    base::Days(7),                                // new_device_threshold
    prefs::kHatsAudioOutputProcDeviceIsSelected,  // is_selected_pref_name
    prefs::
        kHatsAudioOutputProcSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Bluetooth Audio Survey -- shown after the user closed an audio stream
// sent to a Bluetooth device after listening for more than one minute.
const HatsConfig kHatsBluetoothAudioSurvey = {
    ::features::kHappinessTrackingSystemBluetoothAudio,  // feature
    base::Days(90),                                      // new_device_threshold
    prefs::kHatsBluetoothAudioDeviceIsSelected,  // is_selected_pref_name
    prefs::
        kHatsBluetoothAudioSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};
// Personalization Avatar Survey -- shown 60 seconds after a user closes the
// Avatar selection page of either OS Settings or Personalization Hub, depending
// on whether PersonalizationHub feature is enabled.
const HatsConfig kHatsPersonalizationAvatarSurvey = {
    ::features::kHappinessTrackingPersonalizationAvatar,  // feature
    base::Days(1),                                      // new_device_threshold
    prefs::kHatsPersonalizationAvatarSurveyIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPersonalizationAvatarSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Personalization Screensaver Survey -- shown 60 seconds after a user closes
// the Screensaver settings page of either OS Settings or Personalization Hub,
// depending on whether PersonalizationHub feature is enabled.
const HatsConfig kHatsPersonalizationScreensaverSurvey = {
    ::features::kHappinessTrackingPersonalizationScreensaver,  // feature
    base::Days(1),  // new_device_threshold
    prefs::
        kHatsPersonalizationScreensaverSurveyIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPersonalizationScreensaverSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Personalization Wallpaper Survey -- shown 60 seconds after a user closes the
// Wallpaper subpage of the Personalization App.
const HatsConfig kHatsPersonalizationWallpaperSurvey = {
    ::features::kHappinessTrackingPersonalizationWallpaper,  // feature
    base::Days(1),  // new_device_threshold
    prefs::
        kHatsPersonalizationWallpaperSurveyIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPersonalizationWallpaperSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// MediaApp PDF Editing experience survey -- shown after a user clicks `Save`
// after editing a PDF in the MediaApp (Gallery), and the save is complete.
const HatsConfig kHatsMediaAppPdfSurvey = {
    ::features::kHappinessTrackingMediaAppPdf,  // feature
    base::Days(7),                              // new_device_threshold
    prefs::kHatsMediaAppPdfIsSelected,          // hatsIsSelectedPrefName
    prefs::kHatsMediaAppPdfCycleEndTs,          // hatsCycleEndTimestampPrefName
};

// Camera App Survey -- shown after an user captured a photo/video or left the
// app with session > 15 seconds.
const HatsConfig kHatsCameraAppSurvey = {
    ::features::kHappinessTrackingSystemCameraApp,  // feature
    base::Days(90),                                 // new_device_threshold
    prefs::kHatsCameraAppDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsCameraAppSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Chromebook Video/Image Editing/Viewing experience survey -- shown after a
// user opens and then subsequently closes the Google Photos Android App.
const HatsConfig kHatsPhotosExperienceSurvey = {
    ::features::kHappinessTrackingPhotosExperience,  // feature
    base::Days(7),                                   // new_device_threshold
    prefs::kHatsPhotosExperienceIsSelected,          // hatsIsSelectedPrefName
    prefs::kHatsPhotosExperienceCycleEndTs,  // hatsCycleEndTimestampPrefName
};

// General Camera Survey -- shown after camera is closed after being open for
// at least 3 minutes by using any app (e.g. Chrome or Android app).
const HatsConfig kHatsGeneralCameraSurvey = {
    ::features::kHappinessTrackingGeneralCamera,  // feature
    base::Days(90),                               // new_device_threshold
    prefs::kHatsGeneralCameraIsSelected,          // is_selected_pref_name
    prefs::kHatsGeneralCameraSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Prioritized General Camera Survey -- shown after camera is closed after being
// open for at least 15 seconds by using any app (e.g. Chrome or Android app).
const HatsConfig kHatsGeneralCameraPrioritizedSurvey = {
    // feature
    ::features::kHappinessTrackingGeneralCameraPrioritized,
    // new_device_threshold
    base::Days(7),
    // is_selected_pref_name
    prefs::kHatsGeneralCameraPrioritizedIsSelected,
    // cycle_end_timestamp_pref_name
    prefs::kHatsGeneralCameraPrioritizedSurveyCycleEndTs,
    // survey_last_interaction_timestamp_pref_name
    prefs::kHatsGeneralCameraPrioritizedLastInteractionTimestamp,
    // threshold_time
    base::Days(120),
};

// Bluetooth revamp experience survey -- shown 5 mins after interacting with new
// Bluetooth UI surfaces.
const HatsConfig kHatsBluetoothRevampSurvey = {
    ::features::kHappinessTrackingSystemBluetoothRevamp,  // feature
    base::Days(1),                          // new_device_threshold
    prefs::kHatsBluetoothRevampIsSelected,  // is_selected_pref_name
    prefs::kHatsBluetoothRevampCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Battery life experience survey -- shown after login.
const HatsConfig kHatsBatteryLifeSurvey = {
    ::features::kHappinessTrackingSystemBatteryLife,  // feature
    base::Days(7),                                    // new_device_threshold
    prefs::kHatsBatteryLifeIsSelected,                // is_selected_pref_name
    prefs::kHatsBatteryLifeCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Peripherals experience survey -- shown after login.
const HatsConfig kHatsPeripheralsSurvey = {
    ::features::kHappinessTrackingSystemPeripherals,  // feature
    base::Days(7),                                    // new_device_threshold
    prefs::kHatsPeripheralsIsSelected,                // is_selected_pref_name
    prefs::kHatsPeripheralsCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Privacy Hub post launch experience survey -- shown 40 seconds after the user
// leaves the Privacy controls page after staying there for 5 seconds.
const HatsConfig kPrivacyHubPostLaunchSurvey = {
    ::features::kHappinessTrackingPrivacyHubPostLaunch,  // feature
    base::Days(1),                                       // new_device_threshold
    prefs::kHatsPrivacyHubPostLaunchIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPrivacyHubPostLaunchCycleEndTs,  // cycle_end_timestamp_pref_name
};

// OS Settings Survey -- shown [5-30] seconds after a user removes focus from
// Settings or closes the Settings app, if user has used Search, it will add it
// as a Product Specific Data (PSD).
const HatsConfig kHatsOsSettingsSearchSurvey = {
    ::features::kHappinessTrackingOsSettingsSearch,  // feature
    base::Days(1),                                   // new_device_threshold
    prefs::kHatsOsSettingsSearchSurveyIsSelected,    // is_selected_pref_name
    prefs::
        kHatsOsSettingsSearchSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Borealis games survey -- Shown after a Steam game exits.
const HatsConfig kHatsBorealisGamesSurvey = {
    ::features::kHappinessTrackingBorealisGames,  // feature
    base::Days(1),                                // new_device_threshold
    prefs::kHatsBorealisGamesSurveyIsSelected,    // is_selected_pref_name
    prefs::kHatsBorealisGamesSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
    prefs::kHatsBorealisGamesLastInteractionTimestamp,
    // survey_last_interaction_timestamp_pref_name
    base::Days(7),  // threshold_time
};

// Launcher survey -- Shown after a user opens the launcher for the first time.
// This survey is enabled for 25% of users.
const HatsConfig kHatsLauncherAppsFindingSurvey = {
    ::features::kHappinessTrackingLauncherAppsFinding,  // feature
    base::Hours(2),                                     // new_device_threshold
    prefs::kHatsLauncherAppsSurveyIsSelected,           // is_selected_pref_name
    prefs::kHatsLauncherAppsSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Launcher survey -- Shown after a user opens the launcher for the first time.
// This survey is enabled for 75% of users.
const HatsConfig kHatsLauncherAppsNeedingSurvey = {
    ::features::kHappinessTrackingLauncherAppsNeeding,  // feature
    base::Hours(2),                                     // new_device_threshold
    prefs::kHatsLauncherAppsSurveyIsSelected,           // is_selected_pref_name
    prefs::kHatsLauncherAppsSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Office integration survey -- Shown after the user opens an Office file:
// For MS365 and Docs/Sheets/Slides, shown when the app is inactive or closed.
// For QuickOffice, shown 1 minute after launch.
const HatsConfig kHatsOfficeSurvey = {
    ::features::kHappinessTrackingOffice,  // feature
    base::Days(1),                         // new_device_threshold
    prefs::kHatsOfficeSurveyIsSelected,    // is_selected_pref_name
    prefs::kHatsOfficeSurveyCycleEndTs,    // cycle_end_timestamp_pref_name
};

}  // namespace ash
