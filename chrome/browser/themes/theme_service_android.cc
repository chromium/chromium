// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_android.h"

#include <cstdint>
#include <utility>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/night_mode/jni_headers/NightModeUtils_jni.h"

// The values of ThemeType.java must match BrowserColorScheme.
static_assert(std::to_underlying(ThemeService::BrowserColorScheme::kSystem) ==
              0);  // ThemeType.SYSTEM_DEFAULT
static_assert(std::to_underlying(ThemeService::BrowserColorScheme::kLight) ==
              1);  // ThemeType.LIGHT
static_assert(std::to_underlying(ThemeService::BrowserColorScheme::kDark) ==
              2);  // ThemeType.DARK

ThemeServiceAndroid::ThemeServiceAndroid(Profile* profile,
                                         const ThemeHelper& theme_helper)
    : ThemeService(profile, theme_helper) {}

ThemeServiceAndroid::~ThemeServiceAndroid() = default;

void ThemeServiceAndroid::SetBrowserColorScheme(
    BrowserColorScheme color_scheme) {
  // Unlike the base class, there is no system theme supplier to swap out, and
  // the profile pref is unused.
  // Changing the setting can detach tabs and recreate activities, so post to
  // avoid doing so from within the caller's stack (e.g. a Mojo dispatch).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ThemeServiceAndroid::ApplyBrowserColorScheme,
                                weak_ptr_factory_.GetWeakPtr(), color_scheme));
}

ThemeService::BrowserColorScheme ThemeServiceAndroid::GetBrowserColorScheme()
    const {
  return static_cast<BrowserColorScheme>(
      night_mode::Java_NightModeUtils_getThemeSetting(
          base::android::AttachCurrentThread()));
}

void ThemeServiceAndroid::ApplyBrowserColorScheme(
    BrowserColorScheme color_scheme) {
  // This synchronously detaches tabs and schedules activity recreation (when
  // the effective night mode changes). Observers are notified afterwards so
  // that GetBrowserColorScheme() returns the new value; WebContents (and their
  // WebUI handlers) survive the recreation.
  night_mode::Java_NightModeUtils_setThemeSetting(
      base::android::AttachCurrentThread(), static_cast<int32_t>(color_scheme));
  // TODO(agrieve): Also notify when the setting is changed from Java (e.g. in
  // Settings), and notify the ThemeServices of other profiles, since the
  // setting is app-wide.
  NotifyThemeChanged();
}
