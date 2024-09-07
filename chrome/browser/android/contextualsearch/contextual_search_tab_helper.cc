// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_tab_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextualSearchTabHelper_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using contextual_search::UnhandledTapWebContentsObserver;

ContextualSearchTabHelper::ContextualSearchTabHelper(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    Profile* profile)
    : weak_java_ref_(env, obj),
      pref_change_registrar_(new PrefChangeRegistrar()) {
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kContextualSearchEnabled,
      base::BindRepeating(
          &ContextualSearchTabHelper::OnContextualSearchPrefChanged,
          weak_factory_.GetWeakPtr()));
  pref_change_registrar_->Add(
      prefs::kContextualSearchWasFullyPrivacyEnabled,
      base::BindRepeating(
          &ContextualSearchTabHelper::OnContextualSearchPrefChanged,
          weak_factory_.GetWeakPtr()));
}

ContextualSearchTabHelper::~ContextualSearchTabHelper() {
  pref_change_registrar_->RemoveAll();
}

void ContextualSearchTabHelper::OnContextualSearchPrefChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_ref_.get(env);
  Java_ContextualSearchTabHelper_onContextualSearchPrefChanged(env, jobj);
}

void ContextualSearchTabHelper::OnShowUnhandledTapUIIfNeeded(int x_px,
                                                             int y_px) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_ref_.get(env);
  Java_ContextualSearchTabHelper_onShowUnhandledTapUIIfNeeded(env, jobj, x_px,
                                                              y_px);
}

void ContextualSearchTabHelper::InstallUnhandledTapNotifierIfNeeded(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jobject>& j_base_web_contents,
    jfloat device_scale_factor) {
  DCHECK(j_base_web_contents);
  content::WebContents* base_web_contents =
      content::WebContents::FromJavaWebContents(j_base_web_contents);
  DCHECK(base_web_contents);

  if (!UnhandledTapWebContentsObserver::FromWebContents(base_web_contents)) {
    // Create an UnhandledTapWebContentsObserver owned by |base_web_contents|.
    UnhandledTapWebContentsObserver::CreateForWebContents(base_web_contents);

    // As per WebContentsUserData::CreateForWebContents(), the constructor of
    // UnhandledTapWebContentsObserver must only accept one parameter holding a
    // pointer to the WebContents that will own it (i.e. |base_web_contents|),
    // forcing us to defer the rest of the initialization to the setters below.
    auto* utwc_observer =
        UnhandledTapWebContentsObserver::FromWebContents(base_web_contents);
    utwc_observer->set_device_scale_factor(device_scale_factor);
    utwc_observer->set_unhandled_tap_callback(base::BindRepeating(
        &ContextualSearchTabHelper::OnShowUnhandledTapUIIfNeeded,
        weak_factory_.GetWeakPtr()));
  }
}

void ContextualSearchTabHelper::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong JNI_ContextualSearchTabHelper_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    Profile* profile) {
  CHECK(profile);
  ContextualSearchTabHelper* tab = new ContextualSearchTabHelper(
      env, obj, profile);
  return reinterpret_cast<intptr_t>(tab);
}
