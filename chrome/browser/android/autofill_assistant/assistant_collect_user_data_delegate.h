// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_

#include "base/android/scoped_java_ref.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for CollectUserDataAction, to react on clicks on its chips.
class AssistantCollectUserDataDelegate {
 public:
  explicit AssistantCollectUserDataDelegate(UiControllerAndroid* ui_controller);
  ~AssistantCollectUserDataDelegate();

  void OnContactInfoChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jcontact_profile);

  void OnShippingAddressChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jaddress);

  void OnCreditCardChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jobject>& jbilling_profile);

  void OnTermsAndConditionsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint state);

  void OnTextLinkClicked(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         jint link);

  void OnLoginChoiceChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jidentifier);

  void OnDateTimeRangeStartDateChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint year,
      jint month,
      jint day);

  void OnDateTimeRangeStartDateCleared(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  void OnDateTimeRangeStartTimeSlotChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint index);

  void OnDateTimeRangeStartTimeSlotCleared(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  void OnDateTimeRangeEndDateChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint year,
      jint month,
      jint day);

  void OnDateTimeRangeEndDateCleared(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  void OnDateTimeRangeEndTimeSlotChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint index);

  void OnDateTimeRangeEndTimeSlotCleared(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  void OnKeyValueChanged(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jkey,
                         const base::android::JavaParamRef<jobject>& jvalue);

  void OnTextFocusLost(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jcaller);

  bool IsContactComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jcontact_profile);

  bool IsShippingAddressComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jaddress);

  bool IsPaymentInstrumentComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jobject>& jaddress);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  UiControllerAndroid* ui_controller_;

  // Java-side AssistantCollectUserDataDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_collect_user_data_delegate_;
};
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_
