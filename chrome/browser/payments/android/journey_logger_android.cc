// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/android/journey_logger_android.h"

#include "base/android/jni_string.h"
#include "components/payments/content/android/jni_headers/JourneyLogger_jni.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"

namespace payments {
namespace {

using ::base::android::JavaParamRef;
using ::base::android::ConvertJavaStringToUTF8;

}  // namespace

JourneyLoggerAndroid::JourneyLoggerAndroid(bool is_incognito,
                                           ukm::SourceId source_id)
    : journey_logger_(is_incognito, source_id) {}

JourneyLoggerAndroid::~JourneyLoggerAndroid() {}

void JourneyLoggerAndroid::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller) {
  delete this;
}

void JourneyLoggerAndroid::SetNumberOfSuggestionsShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jsection,
    jint jnumber,
    jboolean jhas_complete_suggestion) {
  DCHECK_GE(jsection, 0);
  DCHECK_LT(jsection, JourneyLogger::Section::SECTION_MAX);
  journey_logger_.SetNumberOfSuggestionsShown(
      static_cast<JourneyLogger::Section>(jsection), jnumber,
      jhas_complete_suggestion);
}

void JourneyLoggerAndroid::SetCanMakePaymentValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jvalue) {
  journey_logger_.SetCanMakePaymentValue(jvalue);
}

void JourneyLoggerAndroid::SetHasEnrolledInstrumentValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jvalue) {
  journey_logger_.SetHasEnrolledInstrumentValue(jvalue);
}

void JourneyLoggerAndroid::SetEventOccurred(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jevent) {
  DCHECK_GE(jevent, 0);
  DCHECK_LE(static_cast<unsigned int>(jevent),
            static_cast<unsigned int>(JourneyLogger::Event::EVENT_ENUM_MAX));
  journey_logger_.SetEventOccurred(static_cast<JourneyLogger::Event>(jevent));
}

void JourneyLoggerAndroid::SetRequestedInformation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean requested_shipping,
    jboolean requested_email,
    jboolean requested_phone,
    jboolean requested_name) {
  journey_logger_.SetRequestedInformation(requested_shipping, requested_email,
                                          requested_phone, requested_name);
}

void JourneyLoggerAndroid::SetRequestedPaymentMethodTypes(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean requested_basic_card,
    jboolean requested_method_google,
    jboolean requested_method_other) {
  // TODO(crbug.com/1110320) : secure=payment-confirmation payment method is not
  // implemented on Android yet.
  journey_logger_.SetRequestedPaymentMethodTypes(
      requested_basic_card, requested_method_google,
      /*requested_method_secure_payment_confirmation=*/false,
      requested_method_other);
}

void JourneyLoggerAndroid::SetCompleted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetCompleted();
}

void JourneyLoggerAndroid::SetAborted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jreason) {
  DCHECK_GE(jreason, 0);
  DCHECK_LT(jreason, JourneyLogger::AbortReason::ABORT_REASON_MAX);
  journey_logger_.SetAborted(static_cast<JourneyLogger::AbortReason>(jreason));
}

void JourneyLoggerAndroid::SetNotShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jreason) {
  DCHECK_GE(jreason, 0);
  DCHECK_LT(jreason, JourneyLogger::NotShownReason::NOT_SHOWN_REASON_MAX);
  journey_logger_.SetNotShown(
      static_cast<JourneyLogger::NotShownReason>(jreason));
}

void JourneyLoggerAndroid::RecordTransactionAmount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jcurrency,
    const base::android::JavaParamRef<jstring>& jvalue,
    jboolean jcompleted) {
  journey_logger_.RecordTransactionAmount(
      ConvertJavaStringToUTF8(env, jcurrency),
      ConvertJavaStringToUTF8(env, jvalue), jcompleted);
}

void JourneyLoggerAndroid::RecordCheckoutStep(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jstep) {
  journey_logger_.RecordCheckoutStep(
      static_cast<JourneyLogger::CheckoutFunnelStep>(jstep));
}

void JourneyLoggerAndroid::SetTriggerTime(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetTriggerTime();
}

void JourneyLoggerAndroid::SetPaymentAppUkmSourceId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    ukm::SourceId source_id) {
  journey_logger_.SetPaymentAppUkmSourceId(source_id);
}

static jlong JNI_JourneyLogger_InitJourneyLoggerAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean jis_incognito,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);  // Verified in Java before invoking this function.
  return reinterpret_cast<jlong>(new JourneyLoggerAndroid(
      jis_incognito, ukm::GetSourceIdForWebContentsDocument(web_contents)));
}

}  // namespace payments
