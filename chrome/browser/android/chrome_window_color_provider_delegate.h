// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_WINDOW_COLOR_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_CHROME_WINDOW_COLOR_PROVIDER_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/android/window_android.h"
#include "ui/color/color_provider_key.h"

class Profile;
class ThemeService;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class ColorProviderSource;
}  // namespace ui

// Window-scoped ColorProviderKeyDelegate installed on ChromeActivity's
// WindowAndroid. Mirrors Desktop's BrowserWidget by applying Activity night
// mode and Profile/ThemeService policy at the UI root rather than per
// WebContents.
class ChromeWindowColorProviderDelegate
    : public ui::WindowAndroid::ColorProviderKeyDelegate,
      public ThemeServiceObserver,
      public ProfileObserver {
 public:
  // Creates and attaches a ChromeWindowColorProviderDelegate to `window`,
  // replacing any existing delegate.
  //
  // - `profile`: if non-null, must be a regular (non-OTR) profile and
  //   `is_incognito_window` must be false; applies this profile's Chrome
  //   ThemeService (ChromeTabbedActivity). If nullptr and
  //   `is_incognito_window` is false, applies Activity night mode only (Custom
  //   Tabs, Popups, AuthTabs, TWAs, Webapps, WebAPKs).
  // - `is_incognito_window`: true for Incognito-only windows (e.g.
  //   OFF_THE_RECORD ChromeTabbedActivity, Incognito Popup, Incognito CCT).
  //   Passed as a boolean rather than an OTR Profile* so startup does not
  //   eagerly instantiate an OTR profile just to configure the window's theme.
  // - `is_night_mode`: the Activity's effective night mode state.
  static void AttachToWindow(ui::WindowAndroid* window,
                             Profile* profile,
                             bool is_incognito_window,
                             bool is_night_mode);

  // Initializes `web_contents`'s ColorProviderSource from its current
  // TopLevelNativeWindow. Incognito-branded and Enterprise Isolated profiles
  // always receive a process-wide singleton so they stay isolated from
  // mixed-mode windows; regular and ephemeral OTR profiles leave the default
  // source untouched when no window is attached to avoid a redundant
  // notification bounce. No-op if `web_contents` is null or being destroyed.
  static void InitWebContentsColorProviderSource(
      content::WebContents* web_contents);

  // Updates `web_contents`'s ColorProviderSource for `window` (or detaches from
  // the host window when `window` is nullptr). Incognito-branded and Enterprise
  // Isolated profiles stay on their process-wide singleton across
  // attach/reparent/detach; regular and ephemeral OTR profiles observe `window`
  // (falling back to the default source when `window` is nullptr). No-op if
  // `web_contents` is null or being destroyed.
  static void UpdateWebContentsColorProviderSource(
      content::WebContents* web_contents,
      ui::WindowAndroid* window);

  ChromeWindowColorProviderDelegate(ui::WindowAndroid& window,
                                    Profile* profile,
                                    bool is_incognito_window,
                                    bool is_night_mode);
  ChromeWindowColorProviderDelegate(const ChromeWindowColorProviderDelegate&) =
      delete;
  ChromeWindowColorProviderDelegate& operator=(
      const ChromeWindowColorProviderDelegate&) = delete;
  ~ChromeWindowColorProviderDelegate() override;

  // ui::WindowAndroid::ColorProviderKeyDelegate:
  ui::ColorProviderKey CustomizeColorProviderKey(
      ui::ColorProviderKey base_key) const override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  class OtrColorProviderSource;

  static ui::ColorProviderSource* GetColorProviderSourceForWindow(
      const Profile* profile,
      ui::WindowAndroid* window);

  const raw_ref<ui::WindowAndroid> window_;
  const bool is_incognito_window_;
  const bool is_night_mode_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
};

#endif  // CHROME_BROWSER_ANDROID_CHROME_WINDOW_COLOR_PROVIDER_DELEGATE_H_
