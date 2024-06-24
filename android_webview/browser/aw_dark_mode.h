// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DARK_MODE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DARK_MODE_H_

#include "android_webview/browser/aw_settings.h"
#include "base/android/jni_weak_ref.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace android_webview {

// Lifetime: WebView
class AwDarkMode : public content::WebContentsObserver,
                   public base::SupportsUserData::Data {
 public:
  AwDarkMode(JNIEnv* env,
             const jni_zero::JavaRef<jobject>& obj,
             content::WebContents* web_contents);
  ~AwDarkMode() override;

  static AwDarkMode* FromWebContents(content::WebContents* contents);

  void PopulateWebPreferences(blink::web_pref::WebPreferences* web_prefs,
                              int force_dark_mode,
                              int force_dark_behavior,
                              bool algorithmic_darkening_allowed);

  void DetachFromJavaObject(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  bool is_force_dark_applied() const { return is_force_dark_applied_; }
  bool prefers_dark_from_theme() const { return prefers_dark_from_theme_; }

 private:
  // content::WebContentsObserver
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void InferredColorSchemeUpdated(
      std::optional<blink::mojom::PreferredColorScheme> color_scheme) override;

  void PopulateWebPreferencesForPreT(blink::web_pref::WebPreferences* web_prefs,
                                     int force_dark_mode,
                                     int force_dark_behavior);

  bool IsAppUsingDarkTheme();

  bool is_force_dark_applied_ = false;
  bool prefers_dark_from_theme_ = false;

  JavaObjectWeakGlobalRef jobj_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DARK_MODE_H_
