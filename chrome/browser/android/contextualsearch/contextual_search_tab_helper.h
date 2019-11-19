// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_TAB_HELPER_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

class ContextualSearchTabHelper {
 public:
  ContextualSearchTabHelper(JNIEnv* env, jobject obj, Profile* profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Installs the UnhandledTapNotifier Mojo handler if needed.
  // The |j_base_web_contents| is a java WebContents of the base page tab.
  // The |device_scale_factor| is the ratio of pixels to dips.
  void InstallUnhandledTapNotifierIfNeeded(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jobject>& j_base_web_contents,
      jfloat device_scale_factor);

 private:
  ~ContextualSearchTabHelper();

  // Methods that call back to Java.
  // Call when the preferences change.
  void OnContextualSearchPrefChanged();
  // Call when an unhandled tap needs to show the UI for a tap at the given
  // position, with the given |font_size_dips|, and |text_run_length| of the
  // enclosing element.
  void OnShowUnhandledTapUIIfNeeded(int x_px,
                                    int y_px,
                                    int font_size_dips,
                                    int text_run_length);

  JavaObjectWeakGlobalRef weak_java_ref_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<ContextualSearchTabHelper> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ContextualSearchTabHelper);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_TAB_HELPER_H_
