// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_prefs.h"

namespace ash {
namespace ambient {
namespace prefs {

// Unfortunately, this used to be referred to as "ambient animation theme" all
// throughout the code until it became clear that every theme in ambient
// mode would not be implemented as an animation. Since this name is persisted
// in pref service and it's not trivial to migrate it to a different name, its
// string literal has not been changed to keep backwards compatibility. However,
// all references to this concept in the code now use the more generic "ambient
// theme".
constexpr char kAmbientTheme[] = "ash.ambient.animation_theme";

constexpr char kAmbientUiSettings[] = "ash.ambient.ui_settings";

constexpr char kAmbientUiSettingsFieldTheme[] = "theme";
constexpr char kAmbientUiSettingsFieldVideo[] = "video";

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

constexpr char kAmbientModeManagedScreensaverEnabled[] =
    "ash.ambient.managed_screensaver.enabled";

constexpr char kAmbientModeManagedScreensaverIdleTimeoutSeconds[] =
    "ash.ambient.managed_screensaver.idle_timeout_seconds";

constexpr char kAmbientModeManagedScreensaverImageDisplayIntervalSeconds[] =
    "ash.ambient.managed_screensaver.image_display_interval_seconds";

constexpr char kAmbientModeManagedScreensaverImages[] =
    "ash.ambient.managed_screensaver.images";

constexpr char kAmbientModeRunningDurationMinutes[] =
    "ash.ambient.screensaver_duration_minutes";

}  // namespace prefs
}  // namespace ambient
}  // namespace ash
