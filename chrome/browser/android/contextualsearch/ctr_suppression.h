// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CTR_SUPPRESSION_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CTR_SUPPRESSION_H_

#include <stddef.h>

#include "base/android/jni_android.h"
#include "components/contextual_search/core/browser/ctr_aggregator.h"
#include "components/contextual_search/core/browser/weekly_activity_storage.h"

// Provides access to aggregated click-through-rate recording for tap
// suppression.  Implements a Java conduit to the CtrAggregator in the
// Contextual Search component. This allows Java to access the aggregated CTR
// values.
// This class also provides device-specific integer storage through its
// associated Java class as required to implement the WeeklyActivityStorage.
class CtrSuppression : public contextual_search::WeeklyActivityStorage {
 public:
  // Constructs a new CtrSuppression linked to the given Java object.
  CtrSuppression(JNIEnv* env, jobject obj);

  CtrSuppression(const CtrSuppression&) = delete;
  CtrSuppression& operator=(const CtrSuppression&) = delete;

  ~CtrSuppression() override;

  // Calls the destructor.  Should be called when this native object is no
  // longer needed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // These methods all just call through to the |CtrAggregator| method of the
  // same name.
  void RecordImpression(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jboolean did_click);
  jint GetCurrentWeekNumber(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  jboolean HasPreviousWeekData(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jint GetPreviousWeekImpressions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jfloat GetPreviousWeekCtr(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  jboolean HasPrevious28DayData(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetPrevious28DayImpressions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jfloat GetPrevious28DayCtr(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  // WeeklyActivityStorage Overrides.
  int ReadClicksForWeekRemainder(int week_remainder) override;
  int ReadImpressionsForWeekRemainder(int week_remainder) override;
  int ReadOldestWeekWritten() override;
  int ReadNewestWeekWritten() override;
  void WriteClicksForWeekRemainder(int week_remainder, int value) override;
  void WriteImpressionsForWeekRemainder(int week_remainder, int value) override;
  void WriteOldestWeekWritten(int value) override;
  void WriteNewestWeekWritten(int value) override;

 private:
  // The CtrAggregator that we forward requests to.
  std::unique_ptr<contextual_search::CtrAggregator> aggregator_;

  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CTR_SUPPRESSION_H_
