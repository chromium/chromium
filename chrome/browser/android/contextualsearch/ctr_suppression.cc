// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/android/contextualsearch/ctr_suppression.h>
#include <set>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/CtrSuppression_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using contextual_search::CtrAggregator;

CtrSuppression::CtrSuppression(JNIEnv* env, jobject obj)
    : WeeklyActivityStorage(contextual_search::kNumWeeksNeededFor28DayData) {
  java_object_.Reset(env, obj);

  // NOTE: Creating the aggregator needs to be done after setting up the Java
  // object because the constructor will call back through the Java object
  // to access storage.
  aggregator_.reset(new CtrAggregator(*this));
  DCHECK(aggregator_);
}

CtrSuppression::~CtrSuppression() {
  JNIEnv* env = AttachCurrentThread();
  Java_CtrSuppression_clearNativePointer(env, java_object_);
}

// Java conduit

void CtrSuppression::RecordImpression(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jboolean did_click) {
  aggregator_->RecordImpression(did_click);
}

jint CtrSuppression::GetCurrentWeekNumber(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  return aggregator_->GetCurrentWeekNumber();
}

jboolean CtrSuppression::HasPreviousWeekData(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  return aggregator_->HasPreviousWeekData();
}

jint CtrSuppression::GetPreviousWeekImpressions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return aggregator_->GetPreviousWeekImpressions();
}

jfloat CtrSuppression::GetPreviousWeekCtr(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  return aggregator_->GetPreviousWeekCtr();
}

jboolean CtrSuppression::HasPrevious28DayData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return aggregator_->HasPrevious28DayData();
}

jint CtrSuppression::GetPrevious28DayImpressions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return aggregator_->GetPrevious28DayImpressions();
}

jfloat CtrSuppression::GetPrevious28DayCtr(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  return aggregator_->GetPrevious28DayCtr();
}

// WeeklyActivityStorage overrides

int CtrSuppression::ReadStorage(std::string storage_bucket) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_storage_bucket =
      ConvertUTF8ToJavaString(env, storage_bucket);
  return Java_CtrSuppression_readInt(env, java_object_, j_storage_bucket);
}

void CtrSuppression::WriteStorage(std::string storage_bucket, int value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_storage_bucket =
      ConvertUTF8ToJavaString(env, storage_bucket);
  Java_CtrSuppression_writeInt(env, java_object_, j_storage_bucket, value);
}

// Java wrapper boilerplate

void CtrSuppression::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_CtrSuppression_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  CtrSuppression* suppression = new CtrSuppression(env, obj);
  return reinterpret_cast<intptr_t>(suppression);
}
