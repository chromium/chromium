// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PAYMENT_METHODS_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PAYMENT_METHODS_DELEGATE_H_

#include <jni.h>
#include <stdint.h>

#include "build/build_config.h"

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace autofill {

// Delegate that listens to changes made in the settings related to payment
// methods.
// This class is owned by the Java AutofillPaymentMethodsDelegate object.
// The Java delegate is responsible for cleaning this object up.
class AutofillPaymentMethodsDelegate {
 public:
  AutofillPaymentMethodsDelegate(Profile* profile);
  ~AutofillPaymentMethodsDelegate();
  AutofillPaymentMethodsDelegate(const AutofillPaymentMethodsDelegate&) =
      delete;
  AutofillPaymentMethodsDelegate& operator=(
      const AutofillPaymentMethodsDelegate&) = delete;

  // Cleans up and deletes the native delegate object. Called by Java.
  void Cleanup(JNIEnv* env);

  // Trigger enrollment/unenrollment action.
  void OfferVirtualCardEnrollment(JNIEnv* env, int64_t instrumentId);
  void UnenrollVirtualCard(JNIEnv* env, int64_t instrumentId);

 private:
  raw_ptr<Profile> profile_;  // weak reference
};
}  // namespace autofill

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PAYMENT_METHODS_DELEGATE_H_
