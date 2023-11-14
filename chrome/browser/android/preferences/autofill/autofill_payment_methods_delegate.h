// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PAYMENT_METHODS_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PAYMENT_METHODS_DELEGATE_H_

#include <jni.h>
#include <stdint.h>

#include "build/build_config.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

using base::android::JavaParamRef;

class Profile;

namespace autofill {
class PersonalDataManager;
class VirtualCardEnrollmentManager;

namespace payments {
class PaymentsNetworkInterface;
}

// Delegate that listens to changes made in the settings related to payment
// methods.
// This class is owned by the Java AutofillPaymentMethodsDelegate object.
// The Java delegate is responsible for cleaning this object up.
class AutofillPaymentMethodsDelegate {
 public:
  explicit AutofillPaymentMethodsDelegate(Profile* profile);
  ~AutofillPaymentMethodsDelegate();
  AutofillPaymentMethodsDelegate(const AutofillPaymentMethodsDelegate&) =
      delete;
  AutofillPaymentMethodsDelegate& operator=(
      const AutofillPaymentMethodsDelegate&) = delete;

  // Cleans up and deletes the native delegate object. Called by Java.
  void Cleanup(JNIEnv* env);

  // Trigger enrollment/unenrollment action.
  void InitVirtualCardEnrollment(JNIEnv* env,
                                 int64_t instrument_id,
                                 const JavaParamRef<jobject>& jcallback);
  void EnrollOfferedVirtualCard(JNIEnv* env,
                                const JavaParamRef<jobject>& jcallback);
  void UnenrollVirtualCard(JNIEnv* env,
                           int64_t instrument_id,
                           const JavaParamRef<jobject>& jcallback);

  void DeleteSavedCvcs(JNIEnv* env);

 private:
  raw_ptr<Profile> profile_;                            // weak reference
  raw_ptr<PersonalDataManager> personal_data_manager_;  // weak reference
  std::unique_ptr<payments::PaymentsNetworkInterface>
      payments_network_interface_;
  std::unique_ptr<VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PAYMENT_METHODS_DELEGATE_H_
