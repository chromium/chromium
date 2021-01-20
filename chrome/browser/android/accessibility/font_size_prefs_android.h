// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ACCESSIBILITY_FONT_SIZE_PREFS_ANDROID_H_
#define CHROME_BROWSER_ANDROID_ACCESSIBILITY_FONT_SIZE_PREFS_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

class PrefChangeRegistrar;
class PrefService;

/*
 * Native implementation of FontSizePrefs. This class is used to get and set
 * FontScaleFactor and ForceEnableZoom.
 */
class FontSizePrefsAndroid {
 public:
  FontSizePrefsAndroid(JNIEnv* env, jobject obj);
  ~FontSizePrefsAndroid();

  void SetFontScaleFactor(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj,
                          jfloat font);
  float GetFontScaleFactor(JNIEnv* env,
                           const base::android::JavaRef<jobject>& obj);
  void SetForceEnableZoom(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj,
                          jboolean enabled);
  bool GetForceEnableZoom(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj);

 private:
  // Callback for FontScaleFactor changes from pref change registrar.
  void OnFontScaleFactorChanged();
  // Callback for ForceEnableZoom changes from pref change registrar.
  void OnForceEnableZoomChanged();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  PrefService* const pref_service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(FontSizePrefsAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_ACCESSIBILITY_FONT_SIZE_PREFS_ANDROID_H_
