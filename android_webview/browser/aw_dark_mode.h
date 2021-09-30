// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DARK_MODE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DARK_MODE_H_

#include "android_webview/browser/aw_settings.h"
#include "base/android/jni_weak_ref.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android_webview {
class AwDarkMode : public content::WebContentsObserver,
                   public base::SupportsUserData::Data {
 public:
  AwDarkMode(JNIEnv* env, jobject obj, content::WebContents* web_contents);
  ~AwDarkMode() override;

  static AwDarkMode* FromWebContents(content::WebContents* contents);

  void PopulateWebPreferences(blink::web_pref::WebPreferences* web_prefs,
                              int force_dark_mode,
                              int force_dark_behavior);

  void DetachFromJavaObject(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  bool is_dark_mode() const { return is_dark_mode_; }

 private:
  bool is_dark_mode_ = false;

  JavaObjectWeakGlobalRef jobj_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DARK_MODE_H_
