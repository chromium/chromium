// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/browsing_data_counter_bridge.h"

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BrowsingDataCounterBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

BrowsingDataCounterBridge::BrowsingDataCounterBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    Profile* profile,
    jint selected_time_period,
    jint data_type)
    : jobject_(obj) {
  DCHECK_GE(data_type, 0);
  DCHECK_LE(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::MAX_VALUE));
  TRACE_EVENT1("browsing_data",
               "BrowsingDataCounterBridge::BrowsingDataCounterBridge",
               "data_type", data_type);

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          browsing_data::ClearBrowsingDataTab::ADVANCED, &pref)) {
    return;
  }

  profile_ = profile->GetOriginalProfile();
  counter_ = BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref);

  if (!counter_)
    return;

  counter_->InitWithoutPeriodPref(
      profile_->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      CalculateBeginDeleteTime(
          static_cast<browsing_data::TimePeriod>(selected_time_period)),
      base::BindRepeating(&BrowsingDataCounterBridge::onCounterFinished,
                          base::Unretained(this)));
  counter_->Restart();
}

BrowsingDataCounterBridge::~BrowsingDataCounterBridge() = default;

void BrowsingDataCounterBridge::SetSelectedTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint selected_time_period) {
  if (!counter_) {
    return;
  }

  counter_->SetBeginTime(CalculateBeginDeleteTime(
      static_cast<browsing_data::TimePeriod>(selected_time_period)));
}

void BrowsingDataCounterBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

void BrowsingDataCounterBridge::onCounterFinished(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_BrowsingDataCounterBridge_onBrowsingDataCounterFinished(
      env, jobject_,
      browsing_data_counter_utils::GetChromeCounterTextFromResult(result.get(),
                                                                  profile_));
}

static jlong JNI_BrowsingDataCounterBridge_InitWithoutPeriodPref(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    Profile* profile,
    jint selected_time_period,
    jint data_type) {
  return reinterpret_cast<intptr_t>(new BrowsingDataCounterBridge(
      env, obj, profile, selected_time_period, data_type));
}
