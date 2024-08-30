// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_dark_mode.h"

#include "android_webview/browser/aw_contents.h"
#include "android_webview/common/aw_features.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwDarkMode_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {
namespace {
const void* const kAwDarkModeUserDataKey = &kAwDarkModeUserDataKey;
bool sShouldEnableSimplifiedDarkMode = false;

bool IsForceDarkEnabled(content::WebContents* web_contents) {
  AwContents* contents = AwContents::FromWebContents(web_contents);
  return contents && contents->GetViewTreeForceDarkState();
}
}  // namespace

// static
jlong JNI_AwDarkMode_Init(JNIEnv* env,
                          const JavaParamRef<jobject>& caller,
                          const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  DCHECK(web_contents);
  return reinterpret_cast<intptr_t>(new AwDarkMode(env, caller, web_contents));
}

void JNI_AwDarkMode_EnableSimplifiedDarkMode(JNIEnv* env) {
  sShouldEnableSimplifiedDarkMode = true;
}

AwDarkMode* AwDarkMode::FromWebContents(content::WebContents* contents) {
  return static_cast<AwDarkMode*>(
      contents->GetUserData(kAwDarkModeUserDataKey));
}

AwDarkMode::AwDarkMode(JNIEnv* env,
                       const jni_zero::JavaRef<jobject>& obj,
                       content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), jobj_(env, obj) {
  web_contents->SetUserData(kAwDarkModeUserDataKey, base::WrapUnique(this));
}

AwDarkMode::~AwDarkMode() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj = jobj_.get(env);
  if (scoped_obj)
    Java_AwDarkMode_onNativeObjectDestroyed(env, scoped_obj);
}

void AwDarkMode::PopulateWebPreferences(
    blink::web_pref::WebPreferences* web_prefs,
    int force_dark_mode,
    int force_dark_behavior,
    bool algorithmic_darkening_allowed) {
  if (!sShouldEnableSimplifiedDarkMode) {
    PopulateWebPreferencesForPreT(web_prefs, force_dark_mode,
                                  force_dark_behavior);
    return;
  }
  prefers_dark_from_theme_ = IsAppUsingDarkTheme();
  web_prefs->preferred_color_scheme =
      prefers_dark_from_theme_ ? blink::mojom::PreferredColorScheme::kDark
                               : blink::mojom::PreferredColorScheme::kLight;
  web_prefs->force_dark_mode_enabled = false;
  is_force_dark_applied_ = false;
  if (IsForceDarkEnabled(web_contents())) {
    is_force_dark_applied_ = true;
    web_prefs->force_dark_mode_enabled = true;
    web_prefs->preferred_color_scheme =
        blink::mojom::PreferredColorScheme::kDark;
  } else if (prefers_dark_from_theme_) {
    is_force_dark_applied_ = algorithmic_darkening_allowed;
    web_prefs->force_dark_mode_enabled = algorithmic_darkening_allowed;
  }
}

void AwDarkMode::PopulateWebPreferencesForPreT(
    blink::web_pref::WebPreferences* web_prefs,
    int force_dark_mode,
    int force_dark_behavior) {
  prefers_dark_from_theme_ = false;
  switch (force_dark_mode) {
    case AwSettings::ForceDarkMode::FORCE_DARK_OFF:
      is_force_dark_applied_ = false;
      break;
    case AwSettings::ForceDarkMode::FORCE_DARK_ON:
      is_force_dark_applied_ = true;
      break;
    case AwSettings::ForceDarkMode::FORCE_DARK_AUTO: {
      is_force_dark_applied_ = IsForceDarkEnabled(web_contents());
      if (!is_force_dark_applied_)
        prefers_dark_from_theme_ = IsAppUsingDarkTheme();
      break;
    }
  }
  web_prefs->preferred_color_scheme =
      is_force_dark_applied_ ? blink::mojom::PreferredColorScheme::kDark
                             : blink::mojom::PreferredColorScheme::kLight;
  if (is_force_dark_applied_) {
    switch (force_dark_behavior) {
      case AwSettings::ForceDarkBehavior::FORCE_DARK_ONLY: {
        web_prefs->preferred_color_scheme =
            blink::mojom::PreferredColorScheme::kLight;
        web_prefs->force_dark_mode_enabled = true;
        break;
      }
      case AwSettings::ForceDarkBehavior::MEDIA_QUERY_ONLY: {
        web_prefs->preferred_color_scheme =
            blink::mojom::PreferredColorScheme::kDark;
        web_prefs->force_dark_mode_enabled = false;
        break;
      }
      // Blink's behavior is that if the preferred color scheme matches the
      // supported color scheme, then force dark will be disabled, otherwise
      // the preferred color scheme will be reset to 'light'. Therefore
      // when enabling force dark, we also set the preferred color scheme to
      // dark so that dark themed content will be preferred over force
      // darkening.
      case AwSettings::ForceDarkBehavior::PREFER_MEDIA_QUERY_OVER_FORCE_DARK: {
        web_prefs->preferred_color_scheme =
            blink::mojom::PreferredColorScheme::kDark;
        web_prefs->force_dark_mode_enabled = true;
        break;
      }
    }
  } else if (prefers_dark_from_theme_) {
    web_prefs->preferred_color_scheme =
        blink::mojom::PreferredColorScheme::kDark;
    if (base::FeatureList::IsEnabled(
            android_webview::features::kWebViewForceDarkModeMatchTheme)) {
      web_prefs->force_dark_mode_enabled = true;
      is_force_dark_applied_ = true;
    }
  } else {
    web_prefs->preferred_color_scheme =
        blink::mojom::PreferredColorScheme::kLight;
    web_prefs->force_dark_mode_enabled = false;
  }
}

bool AwDarkMode::IsAppUsingDarkTheme() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj = jobj_.get(env);
  if (!scoped_obj)
    return false;
  return Java_AwDarkMode_isAppUsingDarkTheme(env, scoped_obj);
}

void AwDarkMode::DetachFromJavaObject(JNIEnv* env,
                                      const JavaParamRef<jobject>& jcaller) {
  jobj_.reset();
}

void AwDarkMode::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_main_frame)
    return;
  UMA_HISTOGRAM_BOOLEAN("Android.WebView.DarkMode.PrefersDarkFromTheme",
                        prefers_dark_from_theme_);
}

void AwDarkMode::InferredColorSchemeUpdated(
    std::optional<blink::mojom::PreferredColorScheme> color_scheme) {
  if (prefers_dark_from_theme_ && color_scheme.has_value()) {
    UMA_HISTOGRAM_BOOLEAN(
        "Android.WebView.DarkMode.PageDarkenedAccordingToAppTheme",
        color_scheme.value() == blink::mojom::PreferredColorScheme::kDark);
  }
}

}  // namespace android_webview
