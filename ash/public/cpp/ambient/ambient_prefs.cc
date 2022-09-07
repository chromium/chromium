// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/ambient/ambient_prefs.h"

namespace ash {
namespace ambient {
namespace prefs {

constexpr char kAmbientAnimationTheme[] = "ash.ambient.animation_theme";

constexpr char kAmbientBackdropClientId[] = "ash.ambient.backdrop.client.id";

constexpr char kAmbientModeEnabled[] = "settings.ambient_mode.enabled";

constexpr char kAmbientModePhotoSourcePref[] =
    "settings.ambient_mode.photo_source_enum";

constexpr char kAmbientModeLockScreenInactivityTimeoutSeconds[] =
    "ash.ambient.lock_screen_idle_timeout";

constexpr char kAmbientModeLockScreenBackgroundTimeoutSeconds[] =
    "ash.ambient.lock_screen_background_timeout";

constexpr char kAmbientModeAnimationPlaybackSpeed[] =
    "ash.ambient.animation_playback_speed";

constexpr char kAmbientModePhotoRefreshIntervalSeconds[] =
    "ash.ambient.photo_refresh_interval";

}  // namespace prefs
}  // namespace ambient
}  // namespace ash
