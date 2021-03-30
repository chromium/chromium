// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/ctr_suppression.h"

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

int CtrSuppression::ReadClicksForWeekRemainder(int week_remainder) {
  JNIEnv* env = AttachCurrentThread();
  return Java_CtrSuppression_readClicks(env, java_object_, week_remainder);
}

int CtrSuppression::ReadImpressionsForWeekRemainder(int week_remainder) {
  JNIEnv* env = AttachCurrentThread();
  return Java_CtrSuppression_readImpressions(env, java_object_, week_remainder);
}

int CtrSuppression::ReadOldestWeekWritten() {
  JNIEnv* env = AttachCurrentThread();
  return Java_CtrSuppression_readOldestWeek(env, java_object_);
}

int CtrSuppression::ReadNewestWeekWritten() {
  JNIEnv* env = AttachCurrentThread();
  return Java_CtrSuppression_readNewestWeek(env, java_object_);
}

void CtrSuppression::WriteClicksForWeekRemainder(int week_remainder,
                                                 int value) {
  JNIEnv* env = AttachCurrentThread();
  Java_CtrSuppression_writeClicks(env, java_object_, week_remainder, value);
}

void CtrSuppression::WriteImpressionsForWeekRemainder(int week_remainder,
                                                      int value) {
  JNIEnv* env = AttachCurrentThread();
  Java_CtrSuppression_writeImpressions(env, java_object_, week_remainder,
                                       value);
}

void CtrSuppression::WriteOldestWeekWritten(int value) {
  JNIEnv* env = AttachCurrentThread();
  Java_CtrSuppression_writeOldestWeek(env, java_object_, value);
}

void CtrSuppression::WriteNewestWeekWritten(int value) {
  JNIEnv* env = AttachCurrentThread();
  Java_CtrSuppression_writeNewestWeek(env, java_object_, value);
}

// Java wrapper boilerplate

void CtrSuppression::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_CtrSuppression_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  CtrSuppression* suppression = new CtrSuppression(env, obj);
  return reinterpret_cast<intptr_t>(suppression);
}
