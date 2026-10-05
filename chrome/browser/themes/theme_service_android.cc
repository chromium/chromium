// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_android.h"

#include <cstdint>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/night_mode/jni_headers/GlobalNightModeStateController_jni.h"
#include "chrome/browser/ui/android/night_mode/jni_headers/NightModeUtils_jni.h"

// The values of ThemeType.java must match BrowserColorScheme.
static_assert(std::to_underlying(ThemeService::BrowserColorScheme::kSystem) ==
              0);  // ThemeType.SYSTEM_DEFAULT
static_assert(std::to_underlying(ThemeService::BrowserColorScheme::kLight) ==
              1);  // ThemeType.LIGHT
static_assert(std::to_underlying(ThemeService::BrowserColorScheme::kDark) ==
              2);  // ThemeType.DARK

namespace {

// All live instances (one per profile). Accessed only on the UI thread.
base::flat_set<ThemeServiceAndroid*>& GetInstances() {
  static base::NoDestructor<base::flat_set<ThemeServiceAndroid*>> instances;
  return *instances;
}

void SetThemeSetting(ThemeService::BrowserColorScheme color_scheme) {
  // This synchronously detaches tabs and schedules activity recreation (when
  // the effective night mode changes), and notifies observers via
  // GlobalNightModeStateController.
  NightModeUtilsJni::setThemeSetting(jni_zero::AttachCurrentThread(),
                                     static_cast<int32_t>(color_scheme));
}

}  // namespace

ThemeServiceAndroid::ThemeServiceAndroid(Profile* profile,
                                         const ThemeHelper& theme_helper)
    : ThemeService(profile, theme_helper) {
  GetInstances().insert(this);
}

ThemeServiceAndroid::~ThemeServiceAndroid() {
  GetInstances().erase(this);
}

// static
void ThemeServiceAndroid::OnThemeSettingChanged() {
  for (ThemeServiceAndroid* instance : GetInstances()) {
    instance->NotifyThemeChanged();
  }
}

void ThemeServiceAndroid::SetBrowserColorScheme(
    BrowserColorScheme color_scheme) {
  // Unlike the base class, there is no system theme supplier to swap out, and
  // the profile pref is unused.
  // Changing the setting can detach tabs and recreate activities, so post to
  // avoid doing so from within the caller's stack (e.g. a Mojo dispatch).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SetThemeSetting, color_scheme));
}

ThemeService::BrowserColorScheme ThemeServiceAndroid::GetBrowserColorScheme()
    const {
  return static_cast<BrowserColorScheme>(
      NightModeUtilsJni::getThemeSetting(jni_zero::AttachCurrentThread()));
}

namespace night_mode {

static void JNI_GlobalNightModeStateController_OnThemeSettingChanged() {
  ThemeServiceAndroid::OnThemeSettingChanged();
}

}  // namespace night_mode

DEFINE_JNI(GlobalNightModeStateController)
