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
// selected (see AmbientAnimationTheme enum).
ASH_PUBLIC_EXPORT extern const char kAmbientAnimationTheme[];

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

}  // namespace prefs
}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
