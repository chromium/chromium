// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_

namespace ash::ambient::prefs {

// Integer pref corresponding to the ambient mode theme that the user has
// selected (see AmbientTheme enum).
// DEPRECATED: Use |kAmbientUiSettings| instead; that's the successor.
//
// Unfortunately, this used to be referred to as "ambient animation theme" all
// throughout the code until it became clear that every theme in ambient
// mode would not be implemented as an animation. Since this name is persisted
// in pref service and it's not trivial to migrate it to a different name, its
// string literal has not been changed to keep backwards compatibility. However,
// all references to this concept in the code now use the more generic "ambient
// theme".
inline constexpr char kAmbientTheme[] = "ash.ambient.animation_theme";

// Dictionary pref capturing the ambient UI that the user has selected:
// {
//   // Required.
//   "theme": <integer value of |AmbientTheme| enum>
//   // Which video the user picked. Only used if the "theme" is |kVideo|.
//   "video": <integer value of |AmbientVideo| enum>
// }
inline constexpr char kAmbientUiSettings[] = "ash.ambient.ui_settings";

// Fields in the |kAmbientUiSettings| dictionary.
inline constexpr char kAmbientUiSettingsFieldTheme[] = "theme";
inline constexpr char kAmbientUiSettingsFieldVideo[] = "video";

// A GUID for backdrop client.
inline constexpr char kAmbientBackdropClientId[] =
    "ash.ambient.backdrop.client.id";

// Boolean pref for whether ambient mode is enabled.
inline constexpr char kAmbientModeEnabled[] = "settings.ambient_mode.enabled";

// Integer pref for reporting metrics with the histogram
// |Ash.AmbientMode.PhotoSource|. Not displayed to the user in settings.
inline constexpr char kAmbientModePhotoSourcePref[] =
    "settings.ambient_mode.photo_source_enum";

// Integer pref for the number of seconds to wait before starting Ambient mode
// on lock screen. Not displayed to the user in settings.
inline constexpr char kAmbientModeLockScreenInactivityTimeoutSeconds[] =
    "ash.ambient.lock_screen_idle_timeout";

// Integer pref for the number of seconds to wait before locking the screen in
// the background after Ambient mode has started. Not displayed to the user in
// settings.
inline constexpr char kAmbientModeLockScreenBackgroundTimeoutSeconds[] =
    "ash.ambient.lock_screen_background_timeout";

// Float pref for the playback speed of the animation in ambient mode. Currently
// does not apply to slideshow mode. Not displayed to the user in settings.
inline constexpr char kAmbientModeAnimationPlaybackSpeed[] =
    "ash.ambient.animation_playback_speed";

// Integer pref for the interval in seconds to refresh photos. Not displayed to
// the user in settings.
inline constexpr char kAmbientModePhotoRefreshIntervalSeconds[] =
    "ash.ambient.photo_refresh_interval";

// Boolean policy to pref mapping for whether the managed screensaver is
// enabled. This pref takes the value from the ScreensaverLockScreenEnabled
// policy for user profiles, and from the DeviceScreensaverLoginScreenEnabled
// policy for the sign-in profile.
inline constexpr char kAmbientModeManagedScreensaverEnabled[] =
    "ash.ambient.managed_screensaver.enabled";

// Integer policy to pref mapping for the time in seconds that the device will
// wait idle before showing the managed screensaver. This pref takes the value
// from the ScreensaverLockScreenIdleTimeoutSeconds policy for user profiles,
// and from the DeviceScreensaverLoginScreenIdleTimeoutSeconds policy for the
// sign-in profile.
inline constexpr char kAmbientModeManagedScreensaverIdleTimeoutSeconds[] =
    "ash.ambient.managed_screensaver.idle_timeout_seconds";

// Integer policy to pref mapping for the interval in seconds to display an
// image when the managed screensaver has multiple images to display. This pref
// takes the value from the ScreensaverLockScreenImageDisplayIntervalSeconds
// policy for user profiles, and from the
// DeviceScreensaverLoginScreenImageDisplayIntervalSeconds policy for the
// sign-in profile.
inline constexpr char
    kAmbientModeManagedScreensaverImageDisplayIntervalSeconds[] =
        "ash.ambient.managed_screensaver.image_display_interval_seconds";

// List policy to pref mapping for the list of external images sources to
// display in the managed screensaver has multiple images to display.
// This pref takes the value from the ScreensaverLockScreenImages policy
// for user profiles, and from the DeviceScreensaverLoginScreenImages policy
// for the sign-in profile.
inline constexpr char kAmbientModeManagedScreensaverImages[] =
    "ash.ambient.managed_screensaver.images";

// Integer pref for the number of minutes to wait before putting the device into
// sleep after Ambient mode has started. Logged in users can set this value in
// the Personalization Hub screensaver subpage.
inline constexpr char kAmbientModeRunningDurationMinutes[] =
    "ash.ambient.screensaver_duration_minutes";

}  // namespace ash::ambient::prefs

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
