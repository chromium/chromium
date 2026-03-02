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

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using contextual_search::UnhandledTapWebContentsObserver;

ContextualSearchTabHelper::ContextualSearchTabHelper(Profile* profile)
    : pref_change_registrar_(new PrefChangeRegistrar()) {
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
  Java_ContextualSearchTabHelper_onContextualSearchPrefChanged(
      env, GetJavaObject(env));
}

void ContextualSearchTabHelper::OnShowUnhandledTapUIIfNeeded(int x_px,
                                                             int y_px) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchTabHelper_onShowUnhandledTapUiIfNeeded(
      env, GetJavaObject(env), x_px, y_px);
}

void ContextualSearchTabHelper::InstallUnhandledTapNotifierIfNeeded(
    JNIEnv* env,
    const JavaRef<jobject>& j_base_web_contents,
    float device_scale_factor) {
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

void ContextualSearchTabHelper::Destroy(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<jobject> ContextualSearchTabHelper::GetJavaObject(
    JNIEnv* env) const {
  return Java_ContextualSearchTabHelper_getJavaObject(
      env, reinterpret_cast<intptr_t>(this));
}

static int64_t JNI_ContextualSearchTabHelper_Init(JNIEnv* env,
                                                  Profile* profile) {
  CHECK(profile);
  ContextualSearchTabHelper* helper =
      new ContextualSearchTabHelper(profile);
  return reinterpret_cast<intptr_t>(helper);
}

DEFINE_JNI(ContextualSearchTabHelper)
