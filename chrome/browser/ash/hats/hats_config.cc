// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_config.h"

#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"

namespace ash {

namespace {
constexpr int kMinDaysThreshold = 0;  // HaTS Onboarding Experience is immediate
}  // namespace

HatsConfig::HatsConfig(const base::Feature& feature,
                       const char* const histogram_name,
                       const base::TimeDelta& new_device_threshold,
                       const char* const is_selected_pref_name,
                       const char* const cycle_end_timestamp_pref_name)
    : feature(feature),
      histogram_name(histogram_name),
      new_device_threshold(new_device_threshold),
      is_selected_pref_name(is_selected_pref_name),
      cycle_end_timestamp_pref_name(cycle_end_timestamp_pref_name) {
  DCHECK(new_device_threshold.InDaysFloored() >= kMinDaysThreshold);
}

// General Survey -- shown after login
const HatsConfig kHatsGeneralSurvey = {
    ::features::kHappinessTrackingSystem,         // feature
    "Browser.ChromeOS.HatsSatisfaction.General",  // histogram_name
    base::Days(7),                                // new_device_threshold
    prefs::kHatsDeviceIsSelected,                 // is_selected_pref_name
    prefs::kHatsSurveyCycleEndTimestamp,  // cycle_end_timestamp_pref_name
};

// ENT Survey -- shown after login, along with the General Survey
const HatsConfig kHatsEntSurvey = {
    ::features::kHappinessTrackingSystemEnt,  // feature
    "Browser.ChromeOS.HatsSatisfaction.Ent",  // histogram_name
    base::Days(7),                            // new_device_threshold
    prefs::kHatsEntDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsEntSurveyCycleEndTs,          // cycle_end_timestamp_pref_name
};

// Stability Survey -- shown after login, along with the General Survey
const HatsConfig kHatsStabilitySurvey = {
    ::features::kHappinessTrackingSystemStability,  // feature
    "Browser.ChromeOS.HatsSatisfaction.Stability",  // histogram_name
    base::Days(7),                                  // new_device_threshold
    prefs::kHatsStabilityDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsStabilitySurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Performance Survey -- shown after login, along with the General Survey
const HatsConfig kHatsPerformanceSurvey = {
    ::features::kHappinessTrackingSystemPerformance,  // feature
    "Browser.ChromeOS.HatsSatisfaction.Performance",  // histogram_name
    base::Days(7),                                    // new_device_threshold
    prefs::kHatsPerformanceDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsPerformanceSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Onboarding Experience Survey -- shown after completing the Onboarding Dialog
const HatsConfig kHatsOnboardingSurvey = {
    ::features::kHappinessTrackingSystemOnboarding,            // feature
    "Browser.ChromeOS.HatsSatisfaction.OnboardingExperience",  // histogram_name
    base::Minutes(30),                       // new_device_threshold
    prefs::kHatsOnboardingDeviceIsSelected,  // is_selected_pref_name
    prefs::kHatsOnboardingSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Unlock Experience Survey -- shown after successfully unlocking with Smart
// Lock
const HatsConfig kHatsSmartLockSurvey = {
    ::features::kHappinessTrackingSystemSmartLock,  // feature
    "Browser.ChromeOS.HatsSatisfaction.SmartLock",  // histogram_name
    base::Days(7),                                  // hatsNewDeviceThreshold
    prefs::kHatsSmartLockDeviceIsSelected,          // hatsIsSelectedPrefName
    prefs::kHatsSmartLockSurveyCycleEndTs,  // hatsCycleEndTimestampPrefName
};

// Unlock Experience Survey -- shown after successfully unlocking with any auth
// method execpt Smart Lock
const HatsConfig kHatsUnlockSurvey = {
    ::features::kHappinessTrackingSystemUnlock,  // feature
    "Browser.ChromeOS.HatsSatisfaction.Unlock",  // histogram_name
    base::Days(7),                               // hatsNewDeviceThreshold
    prefs::kHatsUnlockDeviceIsSelected,          // hatsIsSelectedPrefName
    prefs::kHatsUnlockSurveyCycleEndTs,  // hatsCycleEndTimestampPrefName
};

// ARC++ Games Survey -- shown after a user played a top XX ARC++ game
const HatsConfig kHatsArcGamesSurvey = {
    ::features::kHappinessTrackingSystemArcGames,  // feature
    "Browser.ChromeOS.HatsSatisfaction.ArcGames",  // histogram_name
    base::Days(7),                                 // new_device_threshold
    prefs::kHatsArcGamesDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsArcGamesSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Audio Survey -- shown after a user closed an audio stream living for more
// than 3 minutes
const HatsConfig kHatsAudioSurvey = {
    ::features::kHappinessTrackingSystemAudio,  // feature
    "Browser.ChromeOS.HatsSatisfaction.Audio",  // histogram_name
    base::Days(90),                             // new_device_threshold
    prefs::kHatsAudioDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsAudioSurveyCycleEndTs,          // cycle_end_timestamp_pref_name
};

// Personalization Avatar Survey -- shown 60 seconds after a user closes the
// Avatar selection page of either OS Settings or Personalization Hub, depending
// on whether PersonalizationHub feature is enabled.
const HatsConfig kHatsPersonalizationAvatarSurvey = {
    ::features::kHappinessTrackingPersonalizationAvatar,        // feature
    "Browser.ChromeOS.HatsSatisfaction.PersonalizationAvatar",  // histogram_name
    base::Days(1),                                      // new_device_threshold
    prefs::kHatsPersonalizationAvatarSurveyIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPersonalizationAvatarSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Personalization Screensaver Survey -- shown 60 seconds after a user closes
// the Screensaver settings page of either OS Settings or Personalization Hub,
// depending on whether PersonalizationHub feature is enabled.
const HatsConfig kHatsPersonalizationScreensaverSurvey = {
    ::features::kHappinessTrackingPersonalizationScreensaver,        // feature
    "Browser.ChromeOS.HatsSatisfaction.PersonalizationScreensaver",  // histogram_name
    base::Days(1),  // new_device_threshold
    prefs::
        kHatsPersonalizationScreensaverSurveyIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPersonalizationScreensaverSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Personalization Wallpaper Survey -- shown 60 seconds after a user closes the
// Wallpaper subpage of the Personalization App.
const HatsConfig kHatsPersonalizationWallpaperSurvey = {
    ::features::kHappinessTrackingPersonalizationWallpaper,        // feature
    "Browser.ChromeOS.HatsSatisfaction.PersonalizationWallpaper",  // histogram_name
    base::Days(1),  // new_device_threshold
    prefs::
        kHatsPersonalizationWallpaperSurveyIsSelected,  // is_selected_pref_name
    prefs::
        kHatsPersonalizationWallpaperSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// MediaApp PDF Editing experience survey -- shown after a user clicks `Save`
// after editing a PDF in the MediaApp (Gallery), and the save is complete.
const HatsConfig kHatsMediaAppPdfSurvey = {
    ::features::kHappinessTrackingMediaAppPdf,        // feature
    "Browser.ChromeOS.HatsSatisfaction.MediaAppPdf",  // histogram_name
    base::Days(7),                                    // new_device_threshold
    prefs::kHatsMediaAppPdfIsSelected,                // hatsIsSelectedPrefName
    prefs::kHatsMediaAppPdfCycleEndTs,  // hatsCycleEndTimestampPrefName
};

// Camera App Survey -- shown after an user captured a photo/video or left the
// app with session > 15 seconds.
const HatsConfig kHatsCameraAppSurvey = {
    ::features::kHappinessTrackingSystemCameraApp,  // feature
    "Browser.ChromeOS.HatsSatisfaction.CameraApp",  // histogram_name
    base::Days(90),                                 // new_device_threshold
    prefs::kHatsCameraAppDeviceIsSelected,          // is_selected_pref_name
    prefs::kHatsCameraAppSurveyCycleEndTs,  // cycle_end_timestamp_pref_name
};

// Chromebook Video/Image Editing/Viewing experience survey -- shown after a
// user opens and then subsequently closes the Google Photos Android App.
const HatsConfig kHatsPhotosExperienceSurvey = {
    ::features::kHappinessTrackingPhotosExperience,        // feature
    "Browser.ChromeOS.HatsSatisfaction.PhotosExperience",  // histogram_name
    base::Days(7),                           // new_device_threshold
    prefs::kHatsPhotosExperienceIsSelected,  // hatsIsSelectedPrefName
    prefs::kHatsPhotosExperienceCycleEndTs,  // hatsCycleEndTimestampPrefName
};

}  // namespace ash
