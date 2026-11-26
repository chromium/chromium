// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_window_color_provider_delegate.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeWindowColorProviderDelegate_jni.h"

// Process-wide ColorProviderSource used to isolate Incognito-branded and
// Enterprise Isolated WebContents from their host WindowAndroid.
//
// Android hosts mixed-mode activities (e.g. ChromeTabbedActivity holding both
// regular and Incognito tabs in the same WindowAndroid). Returning this from
// GetColorProviderSourceForWindow() keeps OTR WebContents on the same source
// across window attach/reparent/detach without extra notifications, while using
// a profile-independent NoDestructor singleton ensures the pointer never
// dangles when OTR profiles or tabs tear down.
class ChromeWindowColorProviderDelegate::OtrColorProviderSource
    : public ui::ColorProviderSource,
      public ui::NativeThemeObserver {
 public:
  explicit OtrColorProviderSource(bool is_incognito)
      : is_incognito_(is_incognito) {
    // Observe the web NativeTheme so OS-level accessibility updates (such as
    // high contrast or forced colors) still propagate to OTR WebContents.
    native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForWeb());
  }
  OtrColorProviderSource(const OtrColorProviderSource&) = delete;
  OtrColorProviderSource& operator=(const OtrColorProviderSource&) = delete;
  ~OtrColorProviderSource() override = default;

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override {
    return ui::ColorProviderManager::Get().GetColorProviderFor(
        GetColorProviderKey());
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    auto key = GetColorProviderKey();
    key.color_mode = color_mode;
    key.forced_colors = forced_colors;
    const ui::ColorProvider* color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    CHECK(color_provider);
    return ui::CreateRendererColorMap(*color_provider);
  }

  ui::ColorProviderKey GetColorProviderKey() const override {
    auto base_key =
        ui::NativeTheme::GetInstanceForWeb()->GetColorProviderKey(nullptr);
    // Delegate to ThemeService's static helper so Incognito/Isolated key rules
    // stay in a single place without requiring a live ThemeService or Profile.
    return ThemeService::GetIncognitoOrIsolatedColorProviderKey(
        std::move(base_key), is_incognito_);
  }

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override {
    NotifyColorProviderChanged();
  }

 private:
  const bool is_incognito_;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
};

// static
void ChromeWindowColorProviderDelegate::AttachToWindow(
    ui::WindowAndroid* window,
    Profile* profile,
    bool is_incognito_window,
    bool is_night_mode) {
  if (!window) {
    return;
  }
  // Transfers ownership of the delegate to `window` and immediately notifies
  // any already-attached WebContents and direct WindowAndroid observers of the
  // updated ColorProviderKey.
  window->SetColorProviderKeyDelegate(
      std::make_unique<ChromeWindowColorProviderDelegate>(
          *window, profile, is_incognito_window, is_night_mode));
}

// static
ui::ColorProviderSource*
ChromeWindowColorProviderDelegate::GetColorProviderSourceForWindow(
    const Profile* profile,
    ui::WindowAndroid* window) {
  // Regular and non-Incognito OTR profiles (such as ephemeral CCTs) inherit
  // their host WindowAndroid's ColorProviderSource dynamically across
  // reparenting.
  if (profile) {
    if (profile->IsIncognitoBranded()) {
      static base::NoDestructor<OtrColorProviderSource> incognito_source(
          /*is_incognito=*/true);
      return incognito_source.get();
    }
    if (profile->IsEnterpriseIsolatedModeProfile()) {
      static base::NoDestructor<OtrColorProviderSource> isolated_source(
          /*is_incognito=*/false);
      return isolated_source.get();
    }
  }
  return window;
}

// static
void ChromeWindowColorProviderDelegate::InitWebContentsColorProviderSource(
    content::WebContents* web_contents) {
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    return;
  }
  Profile* profile = webui::GetUserProfile(web_contents);
  // Only push a non-null initial source: when `GetTopLevelNativeWindow()` is
  // null for a regular/ephemeral profile (e.g. headless/windowless tabs or
  // unit tests), WebContents already starts on DefaultColorProviderSource, so
  // calling SetColorProviderSource(nullptr) would cause a redundant
  // Default -> nullptr -> Default notification bounce.
  if (ui::ColorProviderSource* source = GetColorProviderSourceForWindow(
          profile, web_contents->GetTopLevelNativeWindow())) {
    web_contents->SetColorProviderSource(source);
  }
}

// static
void ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
    content::WebContents* web_contents,
    ui::WindowAndroid* window) {
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    return;
  }
  Profile* profile = webui::GetUserProfile(web_contents);
  web_contents->SetColorProviderSource(
      GetColorProviderSourceForWindow(profile, window));
}

ChromeWindowColorProviderDelegate::ChromeWindowColorProviderDelegate(
    ui::WindowAndroid& window,
    Profile* profile,
    bool is_incognito_window,
    bool is_night_mode)
    : window_(window),
      is_incognito_window_(is_incognito_window),
      is_night_mode_(is_night_mode) {
  // Incognito-only windows pass `is_incognito_window = true` with a null
  // `profile` so startup does not eagerly create an OTR Profile. Regular
  // browser windows pass the non-OTR original Profile; unthemed windows (CCT,
  // TWA, WebAPK) pass both false and null.
  CHECK(!is_incognito_window_ || !profile);
  CHECK(!profile || !profile->IsOffTheRecord());
  if (profile) {
    // Observe the Profile to detach ThemeService before profile teardown
    // leaves a dangling pointer if the WindowAndroid outlives the Profile.
    profile_observation_.Observe(profile);
    if (ThemeService* theme_service =
            ThemeServiceFactory::GetForProfile(profile)) {
      theme_service_observation_.Observe(theme_service);
    }
  }
}

ChromeWindowColorProviderDelegate::~ChromeWindowColorProviderDelegate() =
    default;

ui::ColorProviderKey
ChromeWindowColorProviderDelegate::CustomizeColorProviderKey(
    ui::ColorProviderKey base_key) const {
  if (is_incognito_window_) {
    // Incognito-only windows always render dark grayscale regardless of the
    // Activity's night mode or any user/OS theme settings.
    return ThemeService::GetIncognitoOrIsolatedColorProviderKey(
        std::move(base_key), /*is_incognito=*/true);
  }

  // Pin the base color mode to this Activity's night mode rather than the
  // process-wide NativeTheme, because individual activities (such as a dark
  // Custom Tab on a light OS) can override night mode independently.
  base_key.color_mode = is_night_mode_
                            ? ui::ColorProviderKey::ColorMode::kDark
                            : ui::ColorProviderKey::ColorMode::kLight;

  const Profile* profile = profile_observation_.GetSource();
  const ThemeService* theme_service = theme_service_observation_.GetSource();
  if (!profile || !theme_service) {
    // Unthemed windows (CCT/TWA/WebAPK) and windows whose Profile has already
    // been destroyed follow only the Activity's night mode and OS accent color.
    return base_key;
  }

  return theme_service->GetColorProviderKey(base_key, profile);
}

void ChromeWindowColorProviderDelegate::OnThemeChanged() {
  // Propagate live ThemeService changes (user color, grayscale, policy,
  // BrowserColorScheme) to all attached WebContents and direct WindowAndroid
  // ColorProvider observers.
  window_->NotifyColorProviderChanged();
}

void ChromeWindowColorProviderDelegate::OnProfileWillBeDestroyed(
    Profile* profile) {
  CHECK_EQ(profile, profile_observation_.GetSource());
  // Detach ThemeService before keyed services are torn down, then notify
  // observers so the window falls back to Activity night mode without touching
  // the destroyed Profile.
  theme_service_observation_.Reset();
  profile_observation_.Reset();
  window_->NotifyColorProviderChanged();
}

static void JNI_ChromeWindowColorProviderDelegate_Attach(
    ui::WindowAndroid* window,
    Profile* profile,
    bool is_incognito_window,
    bool is_night_mode) {
  ChromeWindowColorProviderDelegate::AttachToWindow(
      window, profile, is_incognito_window, is_night_mode);
}

DEFINE_JNI(ChromeWindowColorProviderDelegate)
