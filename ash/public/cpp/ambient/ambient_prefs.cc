// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/logging.h"
#include "components/prefs/pref_service.h"

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

void MigrateDeprecatedPrefs(PrefService& pref_service) {
  // The largest |AmbientTheme| value possible with the old pref
  // |kAmbientTheme|.
  static constexpr ash::AmbientTheme kLegacyMaxAmbientTheme =
      ash::AmbientTheme::kFloatOnBy;

  if (pref_service.HasPrefPath(ash::ambient::prefs::kAmbientUiSettings)) {
    DVLOG(4) << "PrefService has already been migrated to new scheme.";
    // See comments at end of function.
    pref_service.ClearPref(ash::ambient::prefs::kAmbientTheme);
    return;
  }

  if (!pref_service.HasPrefPath(ash::ambient::prefs::kAmbientTheme)) {
    DVLOG(4)
        << "PrefService does not have legacy ambient theme. Nothing to migrate";
    return;
  }

  int current_theme_as_int =
      pref_service.GetInteger(ash::ambient::prefs::kAmbientTheme);
  if (current_theme_as_int < 0 ||
      current_theme_as_int > static_cast<int>(kLegacyMaxAmbientTheme)) {
    // This should be very rare. It can only happen if the pref storage is
    // corrupted somehow.
    LOG(WARNING) << "Loaded invalid ambient theme from pref storage: "
                 << current_theme_as_int << ". Not migrating to new scheme.";
    pref_service.ClearPref(ash::ambient::prefs::kAmbientTheme);
    return;
  }

  // The |kVideo| theme should not be possible here. It's checked above and
  // counted as an invalid theme in the old pref storage.
  base::Value::Dict converted_pref;
  converted_pref.Set(ash::ambient::prefs::kAmbientUiSettingsFieldTheme,
                     current_theme_as_int);
  pref_service.SetDict(ash::ambient::prefs::kAmbientUiSettings,
                       std::move(converted_pref));
  // The legacy pref |kAmbientTheme| is intentionally not erased here to avoid
  // the corner case where erasure of the old pref happens to get committed to
  // storage but the new pref does not, which would lose the old pref value
  // permanently. Wait until the next time MigrateAmbientThemePref() is called,
  // if the new pref is detected, that confirms it's committed permanently to
  // storage, and it's safe to erase the legacy pref.
}

}  // namespace prefs
}  // namespace ambient
}  // namespace ash
