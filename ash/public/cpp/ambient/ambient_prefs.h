// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace ambient {
namespace prefs {

// Integer pref corresponding to the ambient mode theme that the user has
// selected (see AmbientTheme enum).
// DEPRECATED: Use |kAmbientUiSettings| instead; that's the successor.
ASH_PUBLIC_EXPORT extern const char kAmbientTheme[];

// Dictionary pref capturing the ambient UI that the user has selected:
// {
//   // Required.
//   "theme": <integer value of |AmbientTheme| enum>
//   // Which video the user picked. Only used if the "theme" is |kVideo|.
//   "video": <integer value of |AmbientVideo| enum>
// }
ASH_PUBLIC_EXPORT extern const char kAmbientUiSettings[];

// Fields in the |kAmbientUiSettings| dictionary.
ASH_PUBLIC_EXPORT extern const char kAmbientUiSettingsFieldTheme[];
ASH_PUBLIC_EXPORT extern const char kAmbientUiSettingsFieldVideo[];

// A GUID for backdrop client.
ASH_PUBLIC_EXPORT extern const char kAmbientBackdropClientId[];

// Boolean pref for whether ambient mode is enabled.
ASH_PUBLIC_EXPORT extern const char kAmbientModeEnabled[];

// Integer pref for reporting metrics with the histogram
// |Ash.AmbientMode.PhotoSource|. Not displayed to the user in settings.
ASH_PUBLIC_EXPORT extern const char kAmbientModePhotoSourcePref[];

// Integer pref for the number of seconds to wait before starting Ambient mode
// on lock screen. Not displayed to the user in settings.
ASH_PUBLIC_EXPORT extern const char
    kAmbientModeLockScreenInactivityTimeoutSeconds[];

// Integer pref for the number of seconds to wait before locking the screen in
// the background after Ambient mode has started. Not displayed to the user in
// settings.
ASH_PUBLIC_EXPORT extern const char
    kAmbientModeLockScreenBackgroundTimeoutSeconds[];

// Float pref for the playback speed of the animation in ambient mode. Currently
// does not apply to slideshow mode. Not displayed to the user in settings.
ASH_PUBLIC_EXPORT extern const char kAmbientModeAnimationPlaybackSpeed[];

// Integer pref for the interval in seconds to refresh photos. Not displayed to
// the user in settings.
ASH_PUBLIC_EXPORT extern const char kAmbientModePhotoRefreshIntervalSeconds[];

// Boolean policy to pref mapping for whether the managed screensaver is
// enabled. This pref takes the value from the ScreensaverLockScreenEnabled
// policy for user profiles, and from the DeviceScreensaverLoginScreenEnabled
// policy for the sign-in profile.
ASH_PUBLIC_EXPORT extern const char kAmbientModeManagedScreensaverEnabled[];

// Integer policy to pref mapping for the time in seconds that the device will
// wait idle before showing the managed screensaver. This pref takes the value
// from the ScreensaverLockScreenIdleTimeoutSeconds policy for user profiles,
// and from the DeviceScreensaverLoginScreenIdleTimeoutSeconds policy for the
// sign-in profile.
ASH_PUBLIC_EXPORT extern const char
    kAmbientModeManagedScreensaverIdleTimeoutSeconds[];

// Integer policy to pref mapping for the interval in seconds to display an
// image when the managed screensaver has multiple images to display. This pref
// takes the value from the ScreensaverLockScreenImageDisplayIntervalSeconds
// policy for user profiles, and from the
// DeviceScreensaverLoginScreenImageDisplayIntervalSeconds policy for the
// sign-in profile.
ASH_PUBLIC_EXPORT extern const char
    kAmbientModeManagedScreensaverImageDisplayIntervalSeconds[];

// List policy to pref mapping for the list of external images sources to
// display in the managed screensaver has multiple images to display.
// This pref takes the value from the ScreensaverLockScreenImages policy
// for user profiles, and from the DeviceScreensaverLoginScreenImages policy
// for the sign-in profile.
ASH_PUBLIC_EXPORT extern const char kAmbientModeManagedScreensaverImages[];

// Integer pref for the number of minutes to wait before putting the device into
// sleep after Ambient mode has started. Logged in users can set this value in
// the Personalization Hub screensaver subpage.
ASH_PUBLIC_EXPORT extern const char kAmbientModeRunningDurationMinutes[];

}  // namespace prefs
}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
