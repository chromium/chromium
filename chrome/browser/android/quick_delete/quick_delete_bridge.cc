// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/quick_delete/quick_delete_bridge.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/quick_delete/jni_headers/QuickDeleteBridge_jni.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"

using base::android::JavaParamRef;

QuickDeleteBridge::QuickDeleteBridge(Profile* profile) {
  profile_ = profile;

  history_service_ = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

QuickDeleteBridge::~QuickDeleteBridge() = default;

void QuickDeleteBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong JNI_QuickDeleteBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile) {
  QuickDeleteBridge* bridge =
      new QuickDeleteBridge(ProfileAndroid::FromProfileAndroid(j_profile));
  return reinterpret_cast<intptr_t>(bridge);
}
