// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_ANDROID_JOURNEY_LOGGER_ANDROID_H_
#define CHROME_BROWSER_PAYMENTS_ANDROID_JOURNEY_LOGGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/payments/core/journey_logger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace payments {

// Forwarding calls to payments::JourneyLogger.
class JourneyLoggerAndroid {
 public:
  JourneyLoggerAndroid(bool is_incognito, ukm::SourceId source_id);
  ~JourneyLoggerAndroid();

  // Message from Java to destroy this object.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  void SetNumberOfSuggestionsShown(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jsection,
      jint jnumber,
      jboolean jhas_complete_suggestion);
  void IncrementSelectionChanges(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jsection);
  void IncrementSelectionEdits(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jsection);
  void IncrementSelectionAdds(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jsection);
  void SetCanMakePaymentValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jvalue);
  void SetHasEnrolledInstrumentValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jvalue);
  void SetEventOccurred(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcaller,
                        jint jevent);
  void SetRequestedInformation(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean requested_shipping,
      jboolean requested_email,
      jboolean requested_phone,
      jboolean requested_name);
  void SetRequestedPaymentMethodTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean requested_basic_card,
      jboolean requested_method_google,
      jboolean requested_method_other);
  void SetCompleted(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller);
  void SetAborted(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller,
                  jint jreason);
  void SetNotShown(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jcaller,
                   jint jreason);
  void RecordTransactionAmount(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jcurrency,
      const base::android::JavaParamRef<jstring>& jvalue,
      jboolean jcompleted);
  void SetTriggerTime(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  JourneyLogger journey_logger_;

  DISALLOW_COPY_AND_ASSIGN(JourneyLoggerAndroid);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_ANDROID_JOURNEY_LOGGER_ANDROID_H_
