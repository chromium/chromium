// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/browsing_data_counter_bridge.h"

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "chrome/android/chrome_jni_headers/BrowsingDataCounterBridge_jni.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

BrowsingDataCounterBridge::BrowsingDataCounterBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type,
    jint clear_browsing_data_tab)
    : jobject_(obj) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::NUM_TYPES));
  DCHECK_GE(clear_browsing_data_tab, 0);
  DCHECK_LT(clear_browsing_data_tab,
            static_cast<int>(browsing_data::ClearBrowsingDataTab::NUM_TYPES));
  TRACE_EVENT1("browsing_data",
               "BrowsingDataCounterBridge::BrowsingDataCounterBridge",
               "data_type", data_type);

  clear_browsing_data_tab_ =
      static_cast<browsing_data::ClearBrowsingDataTab>(clear_browsing_data_tab);

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          clear_browsing_data_tab_, &pref)) {
    return;
  }

  Profile* profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
  counter_ = BrowsingDataCounterFactory::GetForProfileAndPref(profile, pref);

  if (!counter_)
    return;

  counter_->Init(profile->GetPrefs(), clear_browsing_data_tab_,
                 base::Bind(&BrowsingDataCounterBridge::onCounterFinished,
                            base::Unretained(this)));
  counter_->Restart();
}

BrowsingDataCounterBridge::~BrowsingDataCounterBridge() {
}

void BrowsingDataCounterBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

void BrowsingDataCounterBridge::onCounterFinished(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Profile* profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
  ScopedJavaLocalRef<jstring> result_string =
      base::android::ConvertUTF16ToJavaString(
          env, browsing_data_counter_utils::GetChromeCounterTextFromResult(
                   result.get(), profile));
  Java_BrowsingDataCounterBridge_onBrowsingDataCounterFinished(env, jobject_,
                                                               result_string);
}

static jlong JNI_BrowsingDataCounterBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type,
    jint clear_browsing_data_tab) {
  return reinterpret_cast<intptr_t>(new BrowsingDataCounterBridge(
      env, obj, data_type, clear_browsing_data_tab));
}
