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
      const base::android::JavaParamRef<jstring>& jpayer_name,
      const base::android::JavaParamRef<jstring>& jpayer_phone,
      const base::android::JavaParamRef<jstring>& jpayer_email);

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

  void OnTermsAndConditionsLinkClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint link);

  void OnLoginChoiceChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jidentifier);

  void OnDateTimeRangeStartChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint year,
      jint month,
      jint day,
      jint hour,
      jint minute,
      jint second);

  void OnDateTimeRangeEndChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint year,
      jint month,
      jint day,
      jint hour,
      jint minute,
      jint second);

  void OnKeyValueChanged(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jkey,
                         const base::android::JavaParamRef<jstring>& jvalue);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  UiControllerAndroid* ui_controller_;

  // Java-side AssistantCollectUserDataDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_collect_user_data_delegate_;
};
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_
